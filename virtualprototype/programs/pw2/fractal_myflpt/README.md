# `fractal_myflpt` — format flottant maison

Calcul de l'ensemble de Mandelbrot sur OR1300, qui ne possède pas d'unité
flottante. La version `fractal_flpt` utilise le type `float` du C, ce qui force
le compilateur à appeler les routines logicielles de libgcc (`__addsf3`,
`__mulsf3`, `__subsf3`, `__lesf2`). Ce projet remplace ces appels par un format
flottant défini sur mesure, taillé pour ce seul calcul.

---

## 1. Le format

Un `myflpt` est un `int32_t` découpé en deux demi-mots :

```
 31                    16 15                      0
+------------------------+------------------------+
|   exposant, 16 bits    |   mantisse, 16 bits    |
|   complément à deux    |  signe + 15 magnitude  |
+------------------------+------------------------+

        valeur = mantisse × 2^exposant
```

Une valeur non nulle est **normalisée** : `2^14 ≤ |mantisse| < 2^15`. Elle porte
donc toujours 15 bits significatifs, quelle que soit sa magnitude. Le zéro est
le mot entièrement nul, et c'est la seule valeur dont la mantisse vaut 0.

### Trois exemples d'encodage

**1.5**, construit par `create_myflpt(3, -1)` puisque 1.5 = 3 × 2⁻¹ :

```
magnitude 3 = 0b11, bit de poids fort en position 1
il doit arriver en position 14  ->  décalage de 13 vers la gauche
mantisse = 3 << 13 = 24576        exposant = -1 - 13 = -14
vérification : 24576 × 2^-14 = 24576 / 16384 = 1.5          ✓
mot : exposant -14 = 0xFFF2, mantisse 24576 = 0x6000  ->  0xFFF26000
```

**−2.0**, construit par `create_myflpt(-2, 0)` :

```
signe négatif, magnitude 2 = 0b10, bit de poids fort en position 1
décalage de 13  ->  16384, exposant 0 - 13 = -13
le signe est réappliqué à la fin  ->  mantisse = -16384
vérification : -16384 × 2^-13 = -2.0                        ✓
mot : exposant -13 = 0xFFF3, mantisse -16384 = 0xC000  ->  0xFFF3C000
```

**3/512**, construit par `create_myflpt(3, -9)` :

```
magnitude 3, décalage de 13  ->  24576, exposant -9 - 13 = -22
vérification : 24576 × 2^-22 = 0.005859375 = 3/512          ✓
```

Remarquer que `create_myflpt(3, -1)` ne stocke pas le couple `(3, -1)` tel quel :
la normalisation pousse la mantisse jusqu'à remplir ses 15 bits et compense sur
l'exposant. C'est ce qui garantit la précision maximale pour les opérations
suivantes.

---

## 2. Pourquoi 15 bits de magnitude

C'est le cœur du choix de format, et il découle d'une seule contrainte.

### La borne du produit

Pour que le produit de deux mantisses tienne dans un `int32_t` **sans décalage
préalable**, il faut :

```
|ma| · |mb| < 2^31
```

Avec des mantisses normalisées sous 2^k, le pire cas est 2^(2k). La condition
devient `2k ≤ 30`, donc **k ≤ 15**.

Quinze bits est donc le maximum compatible avec une multiplication exacte en une
seule instruction `l.mul`. Au-delà, il faudrait soit jeter des bits avant de
multiplier — ce qui détruirait la précision que le format prétend offrir — soit
décomposer le produit en trois multiplications.

### La conséquence en cascade : plus de recherche de bit

Si les deux mantisses sont dans `[2^14, 2^15)`, leur produit est nécessairement
dans :

```
[2^14 · 2^14 ; 2^15 · 2^15[  =  [2^28 ; 2^30[
```

Deux octaves seulement. Ramener ce produit dans `[2^14, 2^15)` demande donc
toujours un décalage de 14 **ou** de 15, jamais autre chose. Une comparaison
suffit à trancher, et `myflpt_mul` n'a jamais besoin d'appeler la routine de
normalisation.

### Le choix du complément à deux

La mantisse est signée en complément à deux, et non en signe-magnitude comme le
fait IEEE 754 avec son bit 31 séparé.

La raison est l'addition. Une fois les exposants alignés, additionner se réduit
à `ma + mb` : une instruction. Avec un bit de signe séparé, il faudrait comparer
les deux magnitudes, décider laquelle soustraire de laquelle, puis recalculer le
signe du résultat. C'est une part importante de ce qui rend `__addsf3` coûteux.

L'exposant est lui aussi en complément à deux, sans biais : pas de constante à
ajouter puis retrancher à chaque opération.

### Pourquoi l'exposant hérite de 16 bits

Une fois la mantisse fixée à un champ de 16 bits, les 16 restants vont à
l'exposant. C'est surdimensionné : les valeurs manipulées par l'algorithme vont
d'environ 10⁻⁹ à 8, ce qui correspond à des exposants compris entre −44 et −11
environ. Un champ signé de 8 bits suffirait.

Mais ces bits n'ont nulle part où aller de mieux : les rendre à la mantisse
casserait la contrainte de la section précédente. Autant les laisser à
l'exposant, ce qui a l'avantage d'aligner les deux champs sur les demi-mots et
de laisser de la marge si l'on zoome plus tard.

### Ce que le format ne fait pas

| | IEEE 754 binary32 | `myflpt` |
|---|---|---|
| signe | bit 31 séparé | porté par la mantisse |
| exposant | 8 bits, biaisé (excès-127) | 16 bits, complément à deux |
| mantisse | 23 bits + bit caché implicite | 16 bits explicites |
| dénormaux | oui | non |
| NaN, ±Inf | oui | non |
| arrondi | au plus proche pair | troncature |

Chaque ligne « non » est du code que les routines de libgcc exécutent et que
`myflpt` n'a pas. C'est là qu'est le gain de performance, pas dans un réglage
fin des largeurs de champ.

---

## 3. Les fonctions

### `create_myflpt(man, exp)` — normalisation

Construit un `myflpt` normalisé à partir d'une mantisse et d'un exposant
quelconques. C'est le point de passage de toute valeur produite par une
addition.

La normalisation se fait **en une étape**, pas en boucle : `ilog2` donne la
position du bit de poids fort de la magnitude, et le décalage nécessaire est la
différence entre cette position et 14.

```
m = 100,  exp = 0
ilog2(100) = 6        (100 = 0b1100100, bit de poids fort en position 6)
décalage  = 6 - 14 = -8   ->  vers la gauche de 8
mantisse = 100 << 8 = 25600     exposant = 0 - 8 = -8
vérification : 25600 × 2^-8 = 100                            ✓
```

Deux détails d'implémentation :

**La normalisation porte sur la magnitude**, le signe étant réappliqué à la fin.
Si l'on décalait directement une valeur négative, le décalage arithmétique
arrondit vers le bas et peut produire exactement −32768. Or +32768 n'est pas
représentable dans un champ de 16 bits signé : la négation deviendrait fausse.
En normalisant `|man|`, la mantisse reste strictement dans `(−2^15, 2^15)` et
`myflpt_neg` est toujours exacte.

**`ilog2` n'est pas modifiée.** Elle était déjà fournie dans le fichier et
inutilisée ; elle sert exactement à ça.

### `myflpt_mul(a, b)` — multiplication

Une seule multiplication 32×32, exacte, suivie d'un décalage conditionnel.

```
1.5 × (-2.0)
  ma = 24576  (exp -14)      mb = -16384  (exp -13)
  p  = 24576 × (-16384) = -402653184        exact, |p| < 2^31
  e  = -14 + (-13) = -27
  |p| = 402653184 < 2^29  ->  décalage de 14, e += 14
  |p| >> 14 = 24576         e = -13
  signe négatif  ->  mantisse = -24576
  résultat : -24576 × 2^-13 = -3.0                           ✓
```

Aucun appel à `create_myflpt` : la propriété des deux octaves garantit que le
résultat est déjà normalisé.

### `myflpt_add(a, b)` — addition

Aligne les exposants, additionne les mantisses, renormalise.

```
1.5 + 3/512
  a = (24576, -14)        b = (24576, -22)
  écart d'exposant = -14 - (-22) = 8
  la mantisse de b est décalée de 8  ->  24576 >> 8 = 96
  somme = 24576 + 96 = 24672,  exposant -14
  24672 est déjà dans [16384, 32768)  ->  aucune renormalisation
  résultat : 24672 × 2^-14 = 1.505859375
  vérification : 1.5 + 0.005859375 = 1.505859375             ✓
```

Au-delà d'un écart d'exposant de 15, la plus petite des deux valeurs ne peut plus
influencer le résultat : elle est abandonnée directement plutôt que décalée
jusqu'à disparaître. Après alignement, les deux mantisses sont sous 2^15, donc
leur somme ne peut pas déborder.

### `myflpt_gt(a, b)` — comparaison

Compare les deux nombres champ par champ : d'abord les signes, puis les
exposants, puis les mantisses. Grâce à la normalisation, l'exposant est
déterminant dès que les signes sont identiques.

La version précédente construisait la différence `a + (−b)` et testait son signe,
ce qui déclenchait une addition **et** une normalisation complète à chaque
comparaison — trois fois par itération de Mandelbrot.

### `calc_mandelbrot_point_soft` — la boucle

La structure de l'algorithme est inchangée. Une seule optimisation arithmétique
y a été introduite : le doublement de `2·x·y`.

Doubler un flottant revient à incrémenter son exposant. Comme la mantisse occupe
les bits 0 à 15, ajouter 2^16 au mot incrémente l'exposant sans jamais propager
de retenue dans la mantisse. Une addition d'entier remplace donc un appel à
`myflpt_add`, qui aurait aligné des exposants puis renormalisé.

---

## 4. Fichiers

| fichier | rôle |
|---|---|
| `include/fractal_myflpt.h` | définition du format, macros de champ, prototypes |
| `src/fractal_myflpt.c` | arithmétique, boucle de Mandelbrot, couleurs |
| `src/main_myflpt.c` | initialisation VGA, constantes, appel du rendu |

`ilog2` et les quatre fonctions de conversion en couleur sont reprises telles
quelles, conformément à l'énoncé.

## 5. Compilation

```
make          # produit build-release-or1300/fractal_myflpt.mem
make clean
```
