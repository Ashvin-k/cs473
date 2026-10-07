# 2.3 — Format flottant personnalisé

## Objectif

Définir notre propre format flottant sur 32 bits et l'utiliser pour le calcul de
l'ensemble de Mandelbrot. La contrainte posée par l'énoncé est explicite : le
temps d'exécution doit être **inférieur à celui obtenu avec la bibliothèque
intégrée**, c'est-à-dire avec le type `float` du C.

---

## 1. Le point de départ : pourquoi le `float` est lent ici

Le processeur OR1300 ne possède pas d'unité de calcul flottant. Chaque opération
de la version `fractal_flpt` est donc émulée : le compilateur remplace les
additions et multiplications par des appels aux routines logicielles de sa
bibliothèque — `__mulsf3`, `__addsf3`, `__subsf3`, `__lesf2`. Un désassemblage du
binaire montre que le corps de la boucle d'itération n'exécute pratiquement
aucune arithmétique propre : il enchaîne ces appels, dont les corps comptent
plusieurs centaines d'instructions chacun.

Cette taille s'explique entièrement par ce que la norme IEEE 754 impose et que
notre calcul n'utilise jamais :

| Mécanisme IEEE 754 | Utile pour Mandelbrot ? |
|---|---|
| Nombres dénormalisés | non |
| Valeurs NaN et infinies | non |
| Arrondi au plus proche pair (bits de garde et de collage) | non |
| Bit de mantisse implicite, à insérer puis retirer | non |
| Mantisse en signe-magnitude | non |
| Exposant biaisé, à corriger à chaque opération | non |

Notre format est construit en retirant ces postes. **C'est là qu'est le gain** :
on garde une précision comparable au `float` (23 bits de magnitude), mais
chaque opération se réduit à quelques instructions entières.

> **Figure suggérée.** Un extrait annoté du désassemblage de la boucle en `float`,
> faisant apparaître la succession d'appels à libgcc, avec le nombre
> d'instructions de chaque routine en regard. Elle se produit avec
> `or1k-elf-objdump -d` et illustre le problème en une image.

---

## 2. Le format retenu

Un nombre occupe 32 bits : 8 bits d'exposant et 24 bits de mantisse, soit un
bit de signe et 23 bits de magnitude, comme le découpage 1-23-8 de l'énoncé.

```
 31            24 23                                          0
+----------------+---------------------------------------------+
| exposant, 8 b  |  mantisse, 24 bits                          |
| complément à 2 |  complément à 2 (signe + 23 magnitude)      |
+----------------+---------------------------------------------+

        valeur = mantisse × 2^exposant
```

Toute valeur non nulle est **normalisée** : 2²² ≤ |mantisse| < 2²³. Elle porte
donc toujours 23 bits significatifs, quel que soit son ordre de grandeur. Le
zéro est le mot entièrement nul. Plage représentable : environ
2⁻¹⁰⁶ ≤ |valeur| < 2¹⁵⁰.

Deux exemples d'encodage :

```
 1.5  =  3 × 2^-1
       magnitude 3, bit de poids fort en position 1, à amener en position 22
       ->  décalage de 21 vers la gauche
       mantisse = 0x600000,  exposant = -22 = 0xEA
       mot = 0xEA600000
       vérification : 6291456 × 2^-22 = 1.5                    ✓

-2.0  = -2 × 2^0
       magnitude 2, décalage de 21  ->  0x400000, exposant -21 = 0xEB
       le signe est réappliqué après normalisation : -0x400000 = 0xC00000
       mot = 0xEBC00000
```

> **Figure suggérée.** Le schéma du mot de 32 bits ci-dessus, accompagné d'un
> exemple d'encodage complet montrant la valeur réelle, la mantisse, l'exposant
> et le mot hexadécimal.

---

## 3. La justification des largeurs de champ

### La précision d'abord : 23 bits de magnitude

L'itération de Mandelbrot est **chaotique** près du bord de l'ensemble : une
erreur d'arrondi est amplifiée à chaque itération, et finit par changer le
nombre d'itérations avant échappement, donc la couleur du pixel. Une mantisse
courte (15 ou 16 bits, soit une précision relative de l'ordre de 10⁻⁵) produit
nettement plus de pixels faux (voir section 5). La précision est donc la
ressource à maximiser, et la mantisse reçoit le plus de bits possible.

### Un exposant court suffit : 8 bits

La plage effectivement parcourue par le calcul est petite :

- **valeurs maximales** : le test d'échappement garantit |x|, |y| ≤ 2 avant
  chaque multiplication, donc aucun intermédiaire ne dépasse environ 10
  (|x² − y² + cx| ≤ 6, |2xy + cy| ≤ 10) ;
- **valeurs minimales** : il suffit de descendre bien en dessous du pas entre
  pixels (3/512 ≈ 2⁻⁷·⁵). Une valeur plus petite peut être ramenée à zéro sans
  effet visible.

Huit bits couvrent environ 2⁻¹⁰⁶ à 2¹⁵⁰, ce qui est très largement suffisant,
y compris pour zoomer. Chaque bit d'exposant supplémentaire serait un bit de
précision perdu sans aucun bénéfice.

### Le prix : un produit de 46 bits

Avec deux magnitudes de 23 bits, le produit exact fait 46 bits et ne tient plus
dans un registre. Les deux mauvaises solutions sont :

- caster en `uint64_t` : GCC appelle alors la multiplication 64 bits logicielle ;
- décaler les opérandes avant de multiplier (`(a >> 8) * (b >> 8)`) : on retombe
  à 15 bits de précision, et le champ de 23 bits ne sert plus à rien.

Nous décomposons à la place le produit en **trois multiplications 32 bits**
(section 4), ce qui donne exactement les bits de poids fort du produit.

### Le codage du signe

La mantisse est en complément à deux plutôt qu'en signe et magnitude.
L'argument est l'addition, opération la plus fréquente de l'algorithme : une
fois les exposants alignés, additionner deux mantisses en complément à deux se
réduit à une addition entière. Avec un bit de signe séparé, il faudrait comparer
les deux magnitudes, déterminer le sens de la soustraction, puis recalculer le
signe du résultat.

L'exposant est lui aussi en complément à deux et **sans biais**, ce qui évite
d'ajouter puis de retrancher une constante à chaque opération.

---

## 4. L'implémentation

### La normalisation, en une étape au lieu d'une boucle

La position du bit de poids fort est obtenue par `ilog2`, déjà présente dans le
fichier fourni ; le décalage nécessaire s'en déduit par différence avec la
position cible (bit 22). La normalisation opère sur la **valeur absolue**, le
signe étant réappliqué ensuite : la mantisse reste strictement sous 2²³ en
magnitude, donc son opposé est toujours représentable et la négation est
exacte. Les valeurs trop petites sont ramenées à zéro, les valeurs trop grandes
saturent.

```
3/512 = 3 × 2^-9
ilog2(3) = 1  ->  décalage de 22 - 1 = 21 vers la gauche
mantisse = 3 << 21 = 0x600000,  exposant = -9 - 21 = -30
vérification : 6291456 × 2^-30 = 3/512                       ✓
```

### La multiplication

On travaille sur les magnitudes ua et ub, le signe du résultat étant donné par
le XOR des signes. On écrit ub = bh·2¹⁴ + bl et ua = ah·2⁹ + al
(bh, al < 2⁹ ; bl, ah < 2¹⁴). Chaque produit partiel tient alors dans 32 bits, et

```
floor(ua × ub / 2^14) = ua·bh + ((ah·bl + ((al·bl) >> 9)) >> 5)
```

est **exact** : trois `l.mul`, aucune routine 64 bits. Comme les deux magnitudes
sont dans [2²² ; 2²³[, ce résultat est confiné à deux octaves, [2³⁰ ; 2³²[ : la
renormalisation est toujours un décalage de 8 ou de 9, choisi par un seul test
du bit 31, sans appel à `ilog2`.

```
1.5 × (-2.0)
  magnitudes 0x600000 et 0x400000,  exposants -22 et -21
  P = 3 × 2^21 × 2^22 = 3 × 2^43
  floor(P / 2^14) = 3 × 2^29 = 0x60000000  (bit 31 à 0)  ->  décalage de 8
  magnitude 0x600000,  exposant -22 - 21 + 14 + 8 = -21,  signes différents
  résultat : -6291456 × 2^-21 = -3.0                         ✓
```

### L'addition

Alignement des exposants, addition des mantisses, renormalisation. Les deux
mantisses sont d'abord remontées de 7 **bits de garde** (elles restent sous 2³⁰
en magnitude, la somme tient donc dans un `int32_t`) : les bits du plus petit
opérande qui seraient sinon perdus à l'alignement sont conservés, ce qui compte
pour l'annulation dans x² − y². Au-delà d'un écart d'exposant de 30, le plus
petit opérande ne peut plus influencer le résultat et est abandonné directement.

```
1.5 + 3/512
  exposants -22 et -30,  écart de 8
  0x600000 << 7 = 0x30000000,  (0x600000 << 7) >> 8 = 0x00300000
  somme = 0x30300000,  exposant -22 - 7 = -29
  renormalisée : 0x606000 × 2^-22 = 1.505859375              ✓
```

### La comparaison

Elle procède champ par champ : d'abord les signes, puis les exposants, puis les
mantisses. Grâce à la normalisation, l'exposant est déterminant dès que les
signes coïncident. Aucune soustraction ni renormalisation n'est nécessaire.

### Le doublement du terme croisé

Doubler un flottant revient à incrémenter son exposant. Comme l'exposant occupe
les bits de poids fort du mot, ajouter 2²⁴ l'incrémente sans toucher à la
mantisse. Une addition entière remplace donc un appel à la routine d'addition.

---

## 5. Résultats

### Validation du calcul

Un portage bit à bit de l'arithmétique a été comparé à `double` sur PC :

- erreur relative maximale de la multiplication et de l'addition : 2⁻²²
  (troncature, un ulp au plus), sur 200 000 paires aléatoires ;
- sur un échantillon de 16 384 pixels de l'image 512 × 512, le nombre
  d'itérations diffère de celui calculé en `double` pour **2 pixels**.

La même simulation, en tronquant chaque opération à d'autres largeurs de
mantisse, justifie le découpage choisi :

| Découpage (signe-magnitude-exposant) | Pixels différents de `double` |
|---|---|
| 1-15-16 (première version) | 98 / 16 384 |
| **1-23-8 (retenu)** | **2 / 16 384** |
| 1-25-6 | 4 / 16 384 |
| 1-27-4 (exposant trop court) | 58 / 16 384 |

Au-delà de 23 bits, les écarts restants relèvent du bruit sur le bord chaotique
de l'ensemble. En dessous de 5 bits d'exposant, les petits termes du calcul sont
ramenés à zéro et l'image se dégrade.

> **Figure suggérée — indispensable.** Les deux rendus côte à côte,
> `fractal_flpt` et `fractal_myflpt`, avec une légende indiquant leur
> équivalence visuelle. Sans elle, le chiffre de performance n'a aucune valeur :
> on peut toujours aller plus vite en calculant faux.

### Performance

| version | temps de rendu |
|---|---|
| `fractal_flpt` (virgule flottante émulée) | *à compléter* |
| `fractal_myflpt` (format personnalisé) | *à compléter* |
| rapport | *à compléter* |

Mesures effectuées dans des conditions identiques, en chronométrant l'intervalle
entre les deux messages qui encadrent le rendu sur la liaison série.

> **Figure suggérée.** Un histogramme à trois barres comparant `fractal_flpt`,
> `fractal_fxpt` et `fractal_myflpt`.

### Vérification de l'absence d'émulation

Les symboles `__addsf3`, `__mulsf3`, `__subsf3`, `__lesf2`, ainsi que
`__muldi3`, ne doivent pas figurer dans le binaire de `fractal_myflpt`. La
vérification se fait avec `or1k-elf-nm`.

---

## 6. Limites et comparaison avec la virgule fixe

**Précision.** Avec 23 bits de magnitude, la précision relative est proche de
celle du `float` (2⁻²³ contre 2⁻²⁴ avec le bit implicite d'IEEE 754). Pour des
valeurs d'ordre 1, le pas de Q4.28 (2⁻²⁸) reste plus fin : la virgule fixe offre
une résolution absolue constante, le flottant une résolution **relative**
constante.

**L'intérêt du flottant est la plage dynamique.** Il apparaît en zoomant :
lorsque l'écart entre deux pixels voisins descend sous la résolution de la
virgule fixe, deux colonnes adjacentes reçoivent la même valeur de `c` et
l'image se fige, alors que le flottant continue de les distinguer.

> **Figure suggérée — si le temps le permet.** Un même zoom profond rendu dans
> les deux formats.

**Arrondi.** Toutes les opérations tronquent au lieu d'arrondir. C'est un
biais d'au plus un ulp par opération, invisible à ce cadrage et nettement moins
cher qu'un arrondi au plus proche.

---

## Fichiers et compilation

| fichier | rôle |
|---|---|
| `include/fractal_myflpt.h` | définition du format 8-24, macros de champ, prototypes |
| `src/fractal_myflpt.c` | arithmétique, boucle de Mandelbrot, couleurs |
| `src/main_myflpt.c` | initialisation VGA, constantes, appel du rendu |

`ilog2` et les quatre fonctions de conversion en couleur sont reprises telles
quelles, conformément à l'énoncé.

```
make          # produit build-release-or1300/fractal_myflpt.mem
make clean
```
