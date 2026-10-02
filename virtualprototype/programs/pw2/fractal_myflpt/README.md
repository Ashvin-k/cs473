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
| Exposant biaisé, à corriger à chaque opération | non |
| Mantisse en signe-magnitude | non |

Notre format est construit en retirant précisément ces six postes. **C'est là
qu'est le gain**, et non dans un réglage fin des largeurs de champ.

> **Figure suggérée.** Un extrait annoté du désassemblage de la boucle en `float`,
> faisant apparaître la succession d'appels à libgcc, avec le nombre
> d'instructions de chaque routine en regard. Elle se produit avec
> `or1k-elf-objdump -d` et illustre le problème en une image.

---

## 2. Le format retenu

Un nombre occupe 32 bits, découpés sur la frontière des demi-mots :

```
 31                    16 15                      0
+------------------------+------------------------+
|   exposant, 16 bits    |   mantisse, 16 bits    |
|   complément à deux    |  signe + 15 magnitude  |
+------------------------+------------------------+

        valeur = mantisse × 2^exposant
```

Toute valeur non nulle est **normalisée** : la magnitude de sa mantisse
appartient à l'intervalle allant de 2¹⁴ à 2¹⁵ exclu. Elle porte donc toujours
quinze bits significatifs, quelle que soit son ordre de grandeur. Le zéro est le
mot entièrement nul.

Deux exemples d'encodage, tels que les produit la fonction de construction :

```
 1.5  =  3 × 2^-1
       magnitude 3, bit de poids fort en position 1
       il doit arriver en position 14  ->  décalage de 13 vers la gauche
       mantisse = 24576,  exposant = -14
       vérification : 24576 × 2^-14 = 1.5                      ✓

-2.0  =  -2 × 2^0
       magnitude 2, décalage de 13  ->  16384, exposant -13
       le signe est réappliqué après normalisation  ->  -16384
       vérification : -16384 × 2^-13 = -2.0                    ✓
```

On notera que la fonction ne stocke pas le couple fourni tel quel : la
normalisation pousse la mantisse jusqu'à remplir ses quinze bits et compense sur
l'exposant. C'est ce qui garantit la précision maximale pour les opérations
suivantes.

> **Figure suggérée.** Le schéma du mot de 32 bits ci-dessus, accompagné d'un
> exemple d'encodage complet montrant la valeur réelle, la mantisse, l'exposant
> et le mot hexadécimal. C'est la figure qui rend le format compréhensible d'un
> coup d'œil.

---

## 3. La justification des quinze bits de mantisse

Ce choix n'est pas arbitraire. Il découle d'une contrainte unique, et tout le
reste du format en est la conséquence.

### La contrainte

Pour que le produit de deux mantisses tienne dans un entier signé de 32 bits
**sans décalage préalable**, il faut :

```
|ma| · |mb| < 2³¹
```

Avec des mantisses normalisées sous 2^k, le pire cas vaut 2^(2k). La condition
s'écrit donc 2k ≤ 30, soit **k ≤ 15**.

Quinze bits est ainsi la plus grande mantisse compatible avec une multiplication
exacte en une seule instruction machine. Au-delà, il faudrait soit amputer les
opérandes avant de les multiplier — ce qui reviendrait à payer un champ de
mantisse que le calcul n'exploiterait pas, exactement le défaut identifié dans la
version à virgule fixe — soit décomposer le produit en trois multiplications.

### La conséquence en cascade

Cette contrainte produit un second effet, moins évident et tout aussi important.
Puisque les deux mantisses appartiennent à une seule octave, leur produit est
confiné à deux octaves :

```
[2¹⁴ · 2¹⁴ ; 2¹⁵ · 2¹⁵[  =  [2²⁸ ; 2³⁰[
```

Ramener ce produit dans l'intervalle de normalisation demande donc toujours un
décalage de 14 **ou** de 15, jamais autre chose. Une simple comparaison suffit à
trancher, et la multiplication n'a **jamais** besoin de rechercher la position du
bit de poids fort ni d'appeler la routine de normalisation générale.

### Le codage du signe

Nous avons écarté la représentation en signe et magnitude — celle d'IEEE 754, et
celle proposée par défaut dans l'énoncé avec son bit 31 isolé — au profit d'une
mantisse en complément à deux.

L'argument est l'addition, opération la plus fréquente de l'algorithme. Une fois
les exposants alignés, additionner deux mantisses en complément à deux se réduit
à une addition entière : une instruction. Avec un bit de signe séparé, il
faudrait comparer les deux magnitudes, déterminer le sens de la soustraction,
puis recalculer le signe du résultat. Ce traitement représente une part
substantielle du coût de `__addsf3`.

L'exposant est codé de la même manière, en complément à deux et **sans biais**,
ce qui évite d'ajouter puis de retrancher une constante à chaque opération.

### La largeur de l'exposant

Une fois la mantisse fixée à un champ de 16 bits, les 16 restants vont à
l'exposant. C'est surdimensionné : les grandeurs manipulées s'étendent des
petites valeurs issues des annulations près du bord de l'ensemble jusqu'à une
borne de l'ordre de huit, ce qui ne réclame guère plus de sept ou huit bits.

Mais ces bits ne peuvent pas être rendus à la mantisse sans invalider le
raisonnement précédent. Les affecter à l'exposant a au moins deux avantages :
aligner les deux champs sur les demi-mots, et préserver une marge confortable si
l'on souhaite zoomer.

---

## 4. L'implémentation

### La normalisation, en une étape au lieu d'une boucle

C'est l'optimisation la plus rentable. La position du bit de poids fort de la
magnitude est obtenue par la fonction `ilog2`, déjà présente dans le fichier
fourni et jusqu'alors inutilisée ; le décalage nécessaire s'en déduit
immédiatement par différence avec la position cible.

```
m = 100,  exp = 0
ilog2(100) = 6        (100 = 0b1100100)
décalage = 6 - 14 = -8  ->  vers la gauche de 8
mantisse = 25600,  exposant = -8
vérification : 25600 × 2^-8 = 100                            ✓
```

La version naïve y parvient par deux boucles successives qui déplacent la
mantisse d'un bit à la fois. Sur un cœur simple, sans prédiction de branchement,
chaque tour de ces boucles coûte un branchement non prédit et une division
signée — et la normalisation est appelée à chaque addition.

Un détail d'implémentation mérite d'être signalé : la normalisation opère sur la
**valeur absolue**, le signe étant réappliqué en fin de parcours. Décaler
directement une valeur négative produirait, par arrondi vers le bas du décalage
arithmétique, la valeur la plus négative représentable sur seize bits, dont
l'opposé n'est pas représentable : la négation deviendrait fausse. En travaillant
sur la magnitude, la mantisse reste strictement à l'intérieur du domaine et
toutes les opérations de signe demeurent exactes.

### La multiplication

Une seule multiplication 32 × 32, exacte, suivie d'un décalage conditionnel.
Aucun appel à la routine de normalisation, grâce à la propriété des deux octaves
établie en section 3.

```
1.5 × (-2.0)
  mantisses 24576 et -16384, exposants -14 et -13
  produit = -402653184,  exact,  |p| < 2³¹
  |p| < 2²⁹  ->  décalage de 14,  exposant -27 + 14 = -13
  mantisse = -24576
  résultat : -24576 × 2^-13 = -3.0                           ✓
```

### L'addition

Alignement des exposants, addition des mantisses, renormalisation.

```
1.5 + 3/512
  exposants -14 et -22,  écart de 8
  la plus petite mantisse est décalée de 8 : 24576 >> 8 = 96
  somme = 24576 + 96 = 24672,  déjà normalisée
  résultat : 24672 × 2^-14 = 1.505859375                     ✓
```

Au-delà d'un écart d'exposant de quinze, la plus petite des deux valeurs ne peut
plus influencer le résultat : elle est abandonnée directement plutôt que décalée
jusqu'à disparaître. Après alignement, les deux mantisses sont sous 2¹⁵, donc
leur somme ne peut pas déborder.

### La comparaison

Elle procède champ par champ : d'abord les signes, puis les exposants, puis les
mantisses. Grâce à la normalisation, l'exposant est déterminant dès que les
signes coïncident.

La version naïve construit la différence des deux nombres et teste son signe, ce
qui déclenche une addition **et** une normalisation complète à chaque
comparaison, alors que l'algorithme en effectue trois par itération.

### Le doublement du terme croisé

Doubler un flottant revient à incrémenter son exposant. Comme la mantisse occupe
les bits de poids faible du mot, ajouter 2¹⁶ incrémente l'exposant sans jamais
propager de retenue dans le champ de mantisse. Une addition entière remplace
donc un appel à la routine d'addition flottante, qui aurait aligné des exposants
puis renormalisé.

---

## 5. Résultats

### Validation du calcul

La correction du format se vérifie par comparaison visuelle avec la version en
virgule flottante native : les deux rendus doivent être indiscernables, et la
surface occupée par l'ensemble identique.

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
> `fractal_fxpt` et `fractal_myflpt`. Il synthétise les deux exercices et montre
> où se situe chaque approche.

### Vérification de l'absence d'émulation

Un contrôle simple confirme que l'objectif de l'énoncé est atteint sur le fond et
pas seulement sur le chronomètre : les symboles `__addsf3`, `__mulsf3`,
`__subsf3` et `__lesf2` ne figurent plus dans le binaire de `fractal_myflpt`,
alors qu'ils sont présents dans celui de `fractal_flpt`. La vérification se fait
avec `or1k-elf-nm`.

---

## 6. Limites et comparaison avec la virgule fixe

**La précision est inférieure à celle de Q4.28 à ce niveau de zoom.** Ce n'est
pas un défaut d'implémentation mais une propriété des deux familles de formats.
La virgule fixe offre une résolution absolue constante ; le flottant offre une
résolution **relative** constante, donc une résolution absolue proportionnelle à
la magnitude. Pour des valeurs d'ordre 1, qui dominent le calcul à ce cadrage, le
pas de Q4.28 est plus fin que celui d'une mantisse de quinze bits.

**L'intérêt du flottant est la plage dynamique, pas la précision.** Son avantage
apparaît en zoomant : lorsque l'écart entre deux pixels voisins descend sous la
résolution de la virgule fixe, deux colonnes adjacentes reçoivent la même valeur
de `c` et l'image se fige, alors que le flottant continue de les distinguer.

> **Figure suggérée — si le temps le permet.** Un même zoom profond rendu dans
> les deux formats. Si la virgule fixe produit des aplats uniformes là où le
> flottant conserve du détail, c'est la démonstration visuelle de l'intérêt du
> second. C'est l'argument qui justifie l'existence de cet exercice.

**Le champ d'exposant est surdimensionné.** Sept ou huit bits suffiraient à ce
cadrage. Les bits excédentaires ne peuvent cependant pas être transférés à la
mantisse sans perdre la multiplication en une instruction ; ils constituent une
marge pour le zoom plutôt qu'un gaspillage.

---

## Fichiers et compilation

| fichier | rôle |
|---|---|
| `include/fractal_myflpt.h` | définition du format, macros de champ, prototypes |
| `src/fractal_myflpt.c` | arithmétique, boucle de Mandelbrot, couleurs |
| `src/main_myflpt.c` | initialisation VGA, constantes, appel du rendu |

`ilog2` et les quatre fonctions de conversion en couleur sont reprises telles
quelles, conformément à l'énoncé.

```
make          # produit build-release-or1300/fractal_myflpt.mem
make clean
```
