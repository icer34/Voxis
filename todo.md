## État actuel (constaté en relisant tout le code)

Le `Vertex` dans mesh.h a déjà été changé en `{ uint32_t packedData; }`, mais rien d'autre n'a suivi :
- `chunk_mesher.h` pousse encore `{ v0, normal }` (deux `glm::vec3`) -> ne compile plus contre le nouveau `Vertex`.
- `renderer.cpp` (vertex input attribs) référence encore `offsetof(Vertex, pos)`/`offsetof(Vertex, normal)` -> pareil.
- `shader.vert` prend encore `vec3 inPosition, vec3 inNormal`, et son bloc push_constant n'a que `mat4 model`
  (ne correspond ni à `PushConstants` (3 champs) ni au bloc push_constant de `shader.frag` (3 champs)).
- `shader.frag` hardcode `vec2(0.5, 0.5)` comme UV locale -> aucune vraie UV ne circule encore.
- La section Textures plus bas dans ce fichier (steps 3-6, `_textureSetLayout`/`setBlockAtlas`) décrit un design
  à UN SEUL slot de texture, entièrement remplacé depuis par le bindless (`_bindlessSetLayout`/`_bindlessSet`,
  `MAX_BINDLESS_TEXTURES`) qui est ce qui existe réellement dans renderer.h/renderer.cpp/shader.frag aujourd'hui.
  -> cette ancienne section est obsolète, gardée en bas pour référence historique seulement.
- `World` est une coquille vide (aucun membre). `Game` a un unique `Chunk` de test codé en dur, rempli à 100%
  de blockID `1`, et appelle `_blockAtlas->tileIndex("stone")` directement par nom (aucun registre blockID->nom
  n'existe, le lien est court-circuité à la main pour le test).
- `ChunkMesher` est header-only (namespace anonyme dans chunk_mesher.h), pas de .cpp.

Donc avant le greedy meshing : il faut d'abord finir de câbler le format de vertex packé de bout en bout
avec le mesher NAÏF actuel (pas encore greedy), pour avoir une base qui compile et qui s'affiche correctement.
Le greedy meshing vient après, en changeant uniquement CE QUI GÉNÈRE les vertices, pas le format lui-même.

---

## PHASE 0 — Réparer la base (format de vertex packé de bout en bout, mesher toujours naïf)✅

### 0.1 - Layout final du vertex packé (décision prise, à ne plus rediscuter)✅

Deux `uint32_t` par vertex :

**Mot 1** : `baseX(4 bits) | baseY(4) | baseZ(4) | direction(3) | cornerID(2) | tileIndex(15 bits restants)`
- baseX/Y/Z : indice du bloc de base du quad, `0-15` (PAS la position du coin — voir plus bas)
- direction : `0-5`, une des 6 faces
- cornerID : `0-3`, quel coin du quad ce vertex représente
- tileIndex : jusqu'à 32767 tuiles possibles dans l'atlas (largement assez)

**Mot 2** : `widthMinus1(4 bits) | heightMinus1(4 bits)` (24 bits libres pour plus tard : AO, teinte...)
- largeur/hauteur du quad fusionné, `1-16` stocké comme `valeur - 1` (donc `0-15`, tient sur 4 bits pile)

**Pourquoi ce découpage (rappel de la discussion)** : un coin de quad peut légitimement valoir `16` (toucher le
bord opposé du chunk), ce qui ne rentre pas dans 4 bits (`0-15` seulement). Solution : ne jamais stocker la
position absolue d'un coin, seulement la position de BASE (toujours `0-15`, un vrai indice de bloc) + largeur/
hauteur + cornerID. Le vertex shader reconstruit :
```glsl
vec2 corner2D = vec2(cornerID == 1u || cornerID == 2u ? width  : 0.0,
                      cornerID == 2u || cornerID == 3u ? height : 0.0);
```
Ce même `corner2D` sert à LA FOIS d'offset de position (à combiner avec `base` selon les 2 axes déterminés par
la direction) ET d'UV locale directement (pas besoin de stocker l'UV séparément - largeur/hauteur suffisent).

### 0.2 - `chunk_mesher.cpp` : sortir le mesher du header, corriger `addFace`✅

Crée `chunk_mesher.cpp` (déclaration seule dans `chunk_mesher.h`, comme `mesh.h`/`mesh.cpp`). Le mesher reste
NAÏF pour l'instant (une face = un quad 1x1, pas de fusion) - seul le FORMAT de sortie change :
- `addFace` construit les 2 `uint32_t` packés au lieu de `{vec3 pos, vec3 normal}`.
- Pour un quad 1x1 non fusionné : `width = height = 1` (donc `widthMinus1 = heightMinus1 = 0`), `cornerID` va
  de 0 à 3 pour les 4 vertices de la face.
- `tileIndex` : pour l'instant, une valeur codée en dur (ou passée en paramètre) le temps que le registre
  blockID->nom n'existe pas encore (voir Phase 1.2).

### 0.3 - `renderer.cpp` : attributs de vertex input✅

Remplace les 2 attributs `R32G32B32_SFLOAT` (pos/normal) par 2 attributs `VK_FORMAT_R32_UINT` :
```cpp
vertAttribDesc[0].format = VK_FORMAT_R32_UINT;
vertAttribDesc[0].offset = 0;                        // premier uint32 du struct
vertAttribDesc[1].format = VK_FORMAT_R32_UINT;
vertAttribDesc[1].offset = sizeof(uint32_t);         // deuxième uint32
```
(Il faudra aussi que `Vertex` dans mesh.h ait bien 2 champs `uint32_t` nommés, pas juste un `packedData` seul,
pour que les deux attributs pointent vers des offsets différents.)

### 0.4 - `PushConstants` : retirer `blockTypeIndex`✅

Le block type voyage maintenant par vertex, plus par draw call - `PushConstants` redevient :
```cpp
struct PushConstants
{
    glm::mat4 modelMat;
    uint32_t blockAtlasTextureIndex;
};
```
Mets à jour `game.cpp` (`PushConstants pc{ _testModelMat, _blockAtlas->handle().value };`, plus de 3e argument).

### 0.5 - `shader.vert` : dépacker, reconstruire position + UV, transmettre au fragment shader✅

```glsl
layout(location = 0) in uint inPacked1;
layout(location = 1) in uint inPacked2;

layout(location = 0) flat out uint outTileIndex;
layout(location = 1) out vec2 outUV;

const vec3 NORMALS[6] = vec3[](
    vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1)
);

void main()
{
    uint baseX = inPacked1 & 0xFu;
    uint baseY = (inPacked1 >> 4) & 0xFu;
    uint baseZ = (inPacked1 >> 8) & 0xFu;
    uint direction = (inPacked1 >> 12) & 0x7u;
    uint cornerID = (inPacked1 >> 15) & 0x3u;
    uint tileIndex = (inPacked1 >> 17) & 0x7FFFu;

    uint width = ((inPacked2 & 0xFu)) + 1u;
    uint height = ((inPacked2 >> 4) & 0xFu) + 1u;

    vec2 corner2D = vec2(
        (cornerID == 1u || cornerID == 2u) ? float(width)  : 0.0,
        (cornerID == 2u || cornerID == 3u) ? float(height) : 0.0
    );

    // TODO étape suivante : mapper corner2D sur les 2 bons axes 3D selon `direction`
    // (swizzle) plutôt que de supposer un plan fixe ici
    vec3 localPos = vec3(float(baseX), float(baseY), float(baseZ)); // + offset selon direction, à compléter

    gl_Position = camera.proj * camera.view * pc.model * vec4(localPos, 1.0);
    outTileIndex = tileIndex;
    outUV = corner2D;
}
```
(Le swizzle exact position/direction est à finaliser en Phase 2.6 - ici on peut temporairement ne gérer qu'une
direction pour valider le reste de la chaîne.)

### 0.6 - `shader.frag` : recevoir la vraie UV et le tile index, plus de push constant pour ça✅

```glsl
layout(location = 0) flat in uint inTileIndex;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform constants
{
    mat4 model;
    uint blockAtlasIdx;
} pc;

vec2 atlasUV(uint tileIndex, vec2 localUV)
{
    vec2 wrapped = fract(localUV);   // pour le tiling sur les quads fusionnés plus tard
    uint col = tileIndex % ATLAS_TILES_PER_ROW;
    uint row = tileIndex / ATLAS_TILES_PER_ROW;
    return (vec2(col, row) + wrapped) * ATLAS_TILE_UV_SIZE;
}

void main()
{
    fragColor = texture(bindlessTextures[nonuniformEXT(pc.blockAtlasIdx)], atlasUV(inTileIndex, inUV));
}
```

### 0.7 - Corriger `ATLAS_TILES_PER_ROW`✅

Reste hardcodé (`2u`) pour l'instant tant qu'on a 4 textures - mais note que `BlockAtlas` calcule déjà
`gridSize` dynamiquement (`ceil(sqrt(numTiles))`). Vrai fix plus tard : passer `gridSize` au shader
dynamiquement (uniform/push constant) au lieu d'un `const` qu'il faut resynchroniser à la main à chaque
ajout de texture - à noter en Phase 4, pas bloquant maintenant.

### 0.8 - Build, lance, vérifie visuellement✅

Le chunk de test (rempli à 100%) doit s'afficher comme un gros cube texturé en "stone" sur toutes ses faces
extérieures, chaque face = plusieurs petits quads 1x1 (pas encore fusionnés). Si ça marche, la base est saine.

---

## PHASE 1 — Prérequis côté Chunk avant d'attaquer l'algorithme✅

### 1.1 - Accesseurs sur `Chunk`✅

Ajoute un getter pour `_coords` (position du chunk dans le monde) - nécessaire pour placer le chunk via la
matrice modèle, et pour `World` plus tard. `getBlock`/`setBlock` restent tels quels pour l'instant (le mesher
peut continuer à les utiliser bloc par bloc pour construire les masques - pas besoin d'exposer `_blockIDs`
brut).

### 1.2 - Registre minimal blockID -> nom✅

Un simple `std::unordered_map<uint16_t, std::string>` codé en dur pour l'instant (ex: `{1, "stone"}`,
`{2, "dirt"}`, ...) - pas besoin d'un vrai système de registre extensible tout de suite. Vit où c'est le plus
pratique pour l'instant (`Game`, ou une petite classe `BlockRegistry` si tu préfères déjà l'isoler). Le mesher
l'utilise pour résoudre `blockID -> nom -> BlockAtlas::tileIndex(nom)` une fois par quad fusionné (pas par
vertex).

---

## PHASE 2 — L'algorithme de binary greedy meshing lui-même

### 2.1 - Construire les masques de colonnes par axe

Pour un axe donné (ex: Y), un `uint32_t` par colonne `(x,z)`, bit `y` = bloc solide ou non. Construit les 3
jeux de colonnes (X, Y, Z) à partir de `chunk.getBlock()` - une triple boucle simple, pas besoin d'optimiser.

### 2.2 - Visibilité par décalage de bits

Pour chaque colonne :
```
visibleTop    = column & ~(column >> 1)
visibleBottom = column & ~(column << 1)
```
Répété pour les 3 axes -> 6 masques de visibilité (un par direction), chacun `SIZE x SIZE` colonnes.

### 2.3 - Partitionner par block type

Pour chaque bit posé dans un masque de visibilité, regarder le blockID réel à cette position et le ranger
dans un masque séparé par type (`std::unordered_map<uint16_t, uint32_t>` par colonne, ou construit à la volée
par slice/layer).

### 2.4 - Fonction générique de fusion greedy 2D

Signature suggérée : `std::vector<Rect> greedyMerge2D(const std::array<bool, SIZE*SIZE>& grid)` (ou un
`uint32_t` par ligne si tu veux rester bit à bit). Algorithme :
1. Cherche la première cellule non-consommée à `1`.
2. Étends en largeur tant que la cellule suivante sur la même ligne est `1` et non consommée.
3. Étends en hauteur tant que toute la largeur trouvée reste `1` sur la ligne suivante.
4. Marque le rectangle trouvé comme consommé, ajoute-le au résultat.
5. Répète jusqu'à grille entièrement consommée.

Écris cette fonction ISOLÉMENT (elle ne connaît rien de `Chunk` ni des directions) et teste-la sur une grille
codée en dur avant de la brancher sur le reste - c'est la partie la plus facile à valider indépendamment.

### 2.5 - Par direction, par layer : extraire la grille 2D par type et fusionner

Pour chaque layer le long de l'axe de la direction, pour chaque type présent (étape 2.3), construit la
grille 2D `SIZE x SIZE` de booléens pour CE type à CETTE hauteur, lance `greedyMerge2D` dessus, récupère les
rectangles `{position 2D, largeur, hauteur}`.

### 2.6 - Le swizzle (généraliser aux 6 directions)

Une petite fonction/table qui dit, pour une direction donnée, quel axe du chunk correspond à "largeur de la
grille 2D", "hauteur de la grille 2D", et "profondeur/layer" - pour réutiliser le même code (2.1 à 2.5) pour
les 6 directions au lieu de le dupliquer 6 fois. Fais UNE direction en dur d'abord (ex: +Y), vérifie
visuellement que ça fusionne bien, PUIS généralise via le swizzle.

### 2.7 - Émission des vertices packés

Pour chaque rectangle trouvé (une fois le swizzle en place, pour les 6 directions) : résous
`blockID -> tileIndex` (Phase 1.2), émets 4 vertices packés (format Phase 0.1) + 6 indices, accumule dans un
seul `MeshData` pour tout le chunk (les 6 directions confondues - un seul draw call par chunk au final).

---

## PHASE 3 — Sortir du chunk de test unique (World + Game)

### 3.1 - Implémenter `World`

Stockage des chunks (`std::unordered_map<glm::ivec2, Chunk>` indexé par coordonnées de chunk, cf discussion
précédente sur pourquoi une map spatiale plutôt qu'un ECS pour ça). Méthodes minimales : charger/décharger un
chunk à une position, remesher un chunk (appelle le mesher de la Phase 2, upload via `Renderer::createMesh`,
stocke le `MeshHandle` résultant).

### 3.2 - `Game` possède un `World` au lieu d'un `Chunk` unique

Remplace le chunk de test codé en dur dans `Game::init()` par un `World` qui charge quelques chunks de test.

### 3.3 - Boucle de rendu par frame depuis `World`

`Game::render()` construit la liste `(MeshHandle, PushConstants)` à partir de tous les chunks visibles du
`World` (position monde de chaque chunk -> `modelMat`), au lieu du seul `_testMeshHandle`/`_testModelMat`
actuels.

---

## PHASE 4 — Suivi / points connus, pas bloquants maintenant

- `ATLAS_TILES_PER_ROW` hardcodé dans le shader, doit resynchroniser à la main avec `BlockAtlas::gridSize`
  (calculé dynamiquement) - fragile, à rendre dynamique un jour (uniform/push constant).
- Culling de faces aux frontières entre chunks (un bloc en bordure a besoin de lire le chunk voisin) - pas
  géré du tout actuellement (`isSolid` traite hors-chunk comme vide, donc les faces de bordure s'affichent
  toujours, même si le chunk voisin a un bloc plein juste à côté).
- Padding de l'atlas qui disparaît au-delà du mip level 0 (`PADDING=1`, `1 >> 1 = 0`).
- Mode wireframe pour le debug (`VK_POLYGON_MODE_LINE`, feature `fillModeNonSolid` à activer, deuxième
  pipeline, toggle sur `Key::F3` qui existe déjà et n'est pas utilisé).

# DONE - handles (Handle<Tag> template in renderer.h, MeshHandle/TextureHandle, std::hash specialization)
# DONE - Renderer::createTexture (staging buffer, VkImage, upload barriers, copy, construct in _textures)
# DONE - descriptor set plumbing -> superseded by bindless array, see PHASE 4 note above
# DONE - shader changes -> superseded, see Phase 0.5/0.6 above for the current shader plan
# DONE - per-frame binding -> already correct in recordFrame (binds camera set + bindless set once per frame)
