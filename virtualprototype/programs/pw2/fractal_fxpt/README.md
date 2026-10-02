# 2.2 — Mandelbrot en virgule fixe

## Objectif

Transformer la version en virgule flottante de l'ensemble de Mandelbrot en une
version à virgule fixe, sur un processeur OR1300 dépourvu d'unité flottante. Le
travail consiste à choisir un format Qx.y adapté, puis à réécrire l'arithmétique
de la boucle d'itération avec des entiers 32 bits uniquement.

---

## 1. Le choix du format

### Le compromis à arbitrer

Un format Qx.y répartit 32 bits entre une partie entière et une partie
fractionnaire. C'est un jeu à somme nulle : chaque bit transféré vers la
fraction double la résolution, mais divise par deux la plage représentable. Le
format optimal est donc le plus petit dont la plage contienne toutes les valeurs
que l'algorithme produit — un bit de plus serait du gaspillage de précision, un
bit de moins provoquerait un débordement.

Avec la convention Qi.f où *i* compte le bit de signe et *i* + *f* = 32 :

| Format | Bits entiers (signe inclus) | Plage | Résolution |
|---|---|---|---|
| Q8.24 | 8 | [−128 ; +128[ | 2⁻²⁴ |
| Q5.27 | 5 | [−16 ; +16[ | 2⁻²⁷ |
| **Q4.28** | **4** | **[−8 ; +8[** | **2⁻²⁸** |
| Q3.29 | 3 | [−4 ; +4[ | 2⁻²⁹ |

Le format Q8.24 fourni au départ réserve une plage de ±128. Comme on va le voir,
le calcul n'excède jamais ±10 : quatre bits de précision y sont perdus sans
contrepartie.

### Détermination des bornes

Les paramètres d'entrée sont bornés par la fenêtre de visualisation définie dans
`main_fxpt.c` : l'abscisse `cx` parcourt l'intervalle allant de −2 à 1, et
l'ordonnée `cy` celui allant de −1.5 à 1.5. On a donc |cx| ≤ 2 et |cy| ≤ 1.5.

Le test d'échappement placé en tête du corps de boucle garantit par ailleurs
|x| ≤ 2 et |y| ≤ 2 à l'entrée des multiplications : c'est lui qui rend toutes les
bornes finies. La propagation donne alors, ligne par ligne :

```
xx = x²                  ∈ [0 ; 4]
yy = y²                  ∈ [0 ; 4]
xx + yy                  ≤ 8
2xy                      ≤ 2 · 2 · 2 = 8
x' = xx − yy + cx        ≤ 4 + 2   = 6
y' = 2xy + cy            ≤ 8 + 1.5 = 9.5        ← dimensionnant
```

Trois points méritent une explication :

- `xx − yy` reste dans [−4 ; 4] et non [−8 ; 8], car les deux carrés sont
  positifs : la soustraction ne peut pas les cumuler.
- La borne sur `y'` vaut 9.5 et non 10 parce que |cy| plafonne à 1.5, la fenêtre
  étant moins haute que large.
- `x'` et `y'` doivent être **stockés avant d'être testés** : le test
  d'échappement ne voit les nouvelles valeurs qu'au tour suivant. C'est donc `y'`
  qui dimensionne le format, et non `x`.

### Le format retenu

Le maximum sur l'ensemble des grandeurs est 9.5, ce qui réclamerait en toute
rigueur la plage [−16 ; 16[, soit Q5.27.

Nous avons retenu **Q4.28**, c'est-à-dire un bit de plus de résolution, en
assumant que la borne supérieure n'est pas couverte. La section 3 explique
pourquoi ce choix ne dégrade pas le résultat, et ce qu'il faudrait modifier pour
le rendre rigoureux.

> **Figure suggérée.** Un tableau ou un schéma en barres comparant Q8.24, Q5.27
> et Q4.28 : plage représentable, résolution, et position des bornes réellement
> atteintes par l'algorithme (6 pour `x`, 8 pour `xx + yy`, 9.5 pour `y`). Cette
> figure rend immédiatement visible que Q8.24 est surdimensionné et que Q4.28 est
> à la limite.

---

## 2. L'implémentation de la multiplication

### Où se trouve la virgule après un produit ?

C'est la première question posée par l'énoncé, et elle détermine toute
l'implémentation. En virgule fixe, un réel `A` est stocké comme l'entier
`A × 2^f`. Le produit de deux nombres stockés vaut donc :

```
(A × 2^f) × (B × 2^f)  =  A·B × 2^(2f)
```

Le résultat sort en Q8.56 : les bits fractionnaires s'additionnent. Il faut donc
**56 bits de partie fractionnaire**, soit un produit sur 64 bits, alors que le
processeur ne fournit qu'une multiplication 32 × 32 → 32.

### La solution retenue

Puisqu'il faut retrancher *f* = 28 bits quelque part, nous les retirons **avant**
la multiplication, en décalant chaque opérande de 14 positions :

```
(a >> 14) × (b >> 14)        avec 14 + 14 = 28
```

Le résultat est directement en Q4.28, sans recadrage. Deux vérifications :

**Pas de débordement.** Au moment de la multiplication le test d'échappement
garantit |a| ≤ 2.0, ce qui vaut 2²⁹ en Q4.28. Après décalage, chaque opérande est
borné par 2¹⁵, donc le produit par 2³⁰, qui tient dans un entier signé 32 bits.

**La règle générale.** La somme des deux décalages doit toujours égaler le nombre
de bits fractionnaires du format. C'est l'erreur la plus facile à commettre en
changeant de format : passer de Q8.24 à Q4.28 sans ajuster les décalages produit
des résultats faux d'un facteur deux ou plus, avec une image qui ne ressemble
plus du tout à l'ensemble de Mandelbrot.

### Le coût en précision

Ce schéma a un défaut structurel qu'il faut assumer : chaque opérande est tronqué
à 14 bits significatifs avant d'être multiplié. Plus généralement, avec
`(a >> p) · (b >> q)` et *p* + *q* = *f*, l'opérande le plus tronqué ne conserve
que *f* − max(*p*, *q*) bits, et comme max(*p*, *q*) ≥ *f*/2, **la précision
effective du produit est plafonnée à f/2 bits**, quelle que soit la finesse du
format.

Autrement dit, le format Q4.28 offre 28 bits fractionnaires mais la
multiplication n'en exploite que la moitié. Une décomposition en demi-mots
restituerait le produit complet au prix de trois multiplications au lieu d'une ;
c'est la piste d'amélioration naturelle, discutée en section 5.

---

## 3. Le traitement du débordement

C'est la seconde question posée par l'énoncé : il n'y a aucune détection
matérielle de débordement, comment gérer le problème ?

### La prévention : le test d'échappement en tête de boucle

Le garde-fou placé avant les multiplications interrompt l'itération dès que |x|
ou |y| dépasse 2. Son rôle n'est pas mathématique mais arithmétique : il borne
les opérandes des carrés. Sans lui, une valeur divergente croîtrait sans limite
et ferait déborder le produit en quelques itérations.

Ce test est volontairement placé **avant** le calcul, et non après : il doit
protéger la multiplication, pas constater les dégâts.

### L'acceptation : le cas de `y`

Comme établi en section 1, la grandeur `y` peut atteindre 9.5, ce que Q4.28 ne
représente pas. Le débordement est donc possible, et nous l'avons accepté
sciemment.

La raison est que son effet est inoffensif **pour cet algorithme précis**. Quand
`y` dépasse 8, sa valeur réelle appartient à l'intervalle allant de 8 à 9.5 ;
après repliement en complément à deux sur 32 bits, elle atterrit entre −8 et
−6.5. Sa valeur absolue reste donc largement supérieure au seuil d'échappement de
2, et le point sort de la boucle à l'itération suivante exactement comme il
l'aurait fait sans débordement. Le compteur d'itérations est identique, et
l'image aussi.

Il faut être clair sur ce que cela suppose : le raisonnement repose sur le fait
que toute valeur repliée reste hors du rayon d'échappement. C'est une propriété
du couple algorithme + seuil, pas du format. Il faut aussi noter que le
débordement d'entier signé est un comportement indéfini en C, que le compilateur
a le droit d'exploiter ; une compilation avec `-fwrapv` rendrait la sémantique
explicite.

### La variante rigoureuse

Il existe une manière de rendre Q4.28 irréprochable sans changer de format. Il
suffit d'avancer le test exact `x² + y² < 4` **avant** la mise à jour de `z`.
L'inégalité arithmético-géométrique donne alors gratuitement :

```
2|xy| ≤ x² + y² < 4
```

Le produit croisé passe de 8 à 4, donc |y'| < 4 + 1.5 = 5.5 et |x'| < 6 : tout
rentre dans la plage. Un détail compte dans cette variante : la comparaison doit
s'écrire sans jamais former la somme `xx + yy`, qui peut valoir 8 et n'est donc
pas représentable ; il faut comparer `xx` à `limit − yy`, deux quantités qui
restent dans [0 ; 4].

Nous n'avons pas retenu cette variante dans la version livrée, le déplacement du
test modifiant le moment où le compteur d'itérations s'incrémente.

---

## 4. Résultats

### Validation du calcul

La correction du portage se vérifie par comparaison visuelle avec la version en
virgule flottante : les deux rendus doivent être indiscernables, et la surface
occupée par l'ensemble identique.

> **Figure suggérée — indispensable.** Les deux rendus côte à côte,
> `fractal_flpt` et `fractal_fxpt`. C'est la figure qui établit que le gain de
> performance n'est pas obtenu au prix d'un résultat faux, première question que
> se posera un lecteur.

> **Figure suggérée — optionnelle mais parlante.** Un agrandissement d'une
> portion du bord de l'ensemble, comparant le rendu Q8.24 d'origine et le rendu
> Q4.28. Si des artefacts en marches d'escalier sont visibles dans un cas et pas
> dans l'autre, c'est l'illustration directe de l'effet de la résolution.

### Performance

Le temps d'exécution a été relevé pour les deux versions dans des conditions
identiques, en chronométrant l'intervalle entre les deux messages qui encadrent
le rendu sur la liaison série.

| version | temps de rendu |
|---|---|
| `fractal_flpt` (virgule flottante émulée) | *à compléter* |
| `fractal_fxpt` (virgule fixe Q4.28) | *à compléter* |
| rapport | *à compléter* |

L'origine du gain se lit directement dans le binaire : la version flottante
appelle les routines logicielles `__mulsf3`, `__addsf3`, `__subsf3` et `__lesf2`
à chaque itération, dont les corps comptent plusieurs centaines d'instructions,
tandis que la version à virgule fixe n'exécute que des additions, des décalages
et une multiplication entière. Un désassemblage par `or1k-elf-objdump -d` rend la
différence immédiatement visible.

> **Figure suggérée.** Un extrait annoté du désassemblage des deux boucles
> d'itération, mis en regard. L'une est une succession d'appels de fonction,
> l'autre une suite d'instructions arithmétiques. C'est la justification la plus
> directe du chiffre de performance.

---

## 5. Limites et pistes d'amélioration

**La précision de la multiplication est le facteur limitant**, pas le format. Le
schéma par pré-décalage plafonne la précision effective à la moitié des bits
fractionnaires. Une décomposition de chaque opérande en deux demi-mots de 14 bits
permettrait de calculer le produit complet en restant en 32 bits, au prix de
trois multiplications au lieu d'une. C'est l'amélioration la plus rentable si la
qualité du rendu devenait critique.

**Le débordement de `y` est assumé, non résolu.** La variante décrite en section
3 le supprimerait sans changer de format ni coûter de performance.

**Le format est dimensionné pour ce cadrage seulement.** Avec un pas entre pixels
de 3/512, la résolution de Q4.28 est plusieurs ordres de grandeur plus fine que
nécessaire. En zoomant, c'est elle qui deviendrait limitante : lorsque l'écart
entre deux pixels voisins descend sous 2⁻²⁸, deux colonnes adjacentes reçoivent
la même valeur de `c` et l'image se fige. C'est la limite que la version à
virgule flottante personnalisée, traitée au chapitre suivant, est destinée à
repousser.

---

## Compilation

```
make          # produit build-release-or1300/fractal_fxpt.mem
make clean
```
