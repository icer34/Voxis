# ThreadPool — plan d'implémentation

## Objectif

Une classe `ThreadPool` **générique**, réutilisable pour n'importe quel sous-système (pas seulement
`World`/le chunk streaming — pensée dès le départ pour servir aussi aux entités, au pathfinding, etc.
plus tard). Elle ne doit rien savoir de `Chunk`, `World`, ou de quoi que ce soit de spécifique au jeu.

Usage visé, une fois terminée :

```cpp
ThreadPool pool(std::thread::hardware_concurrency() - 1);

std::future<MeshData> future = pool.submit([&chunk, &neighbors]() {
    return ChunkMesher::getMeshData(chunk, neighbors);
});

// ... plus tard, sur le thread principal ...
MeshData mesh = future.get(); // bloque jusqu'à ce que ce soit prêt
```

Pas de notion de "batch" ou de "job de chunk" dans le pool lui-même — `World` construira son propre
`std::vector<std::future<...>>` par-dessus ce primitif générique.

---

## Décisions de design

- **Nombre de workers fixe** : `std::thread::hardware_concurrency() - 1` (laisser un cœur au thread
  principal), créés une fois à la construction, jamais recréés.
- **`submit()` est générique et retourne une `std::future<T>`** (`T` déduit automatiquement du type de
  retour du callable soumis) — pas de tâches `void()` figées, pas de mécanisme de "batch" custom dans
  le pool. Attendre plusieurs tâches à la fois se fait en collectant plusieurs `future` côté appelant.
- **Chaque tâche est enveloppée dans un `std::packaged_task`** pour créer le lien avec sa `future`.
- **Piège à anticiper** : `std::packaged_task` n'est pas copiable (seulement déplaçable), mais la
  queue de tâches a besoin d'un type homogène et copiable (`std::function<void()>` typiquement
  l'exige). Solution standard : envelopper le `packaged_task` dans un `std::shared_ptr` — copier un
  `shared_ptr` est bon marché (juste le pointeur + compteur de ref), donc la lambda qui le capture
  redevient copiable et peut aller dans un `std::function<void()>`/la queue.
- **Queue protégée par un seul `std::mutex`**, accès concurrent (soumission côté thread principal,
  pop côté workers).
- **Un seul `std::condition_variable`** associé à ce mutex : réveille les workers quand une tâche est
  ajoutée (`notify_one()` à la soumission) ou quand on demande l'arrêt (`notify_all()` dans le
  destructeur).
- **Ne jamais exécuter une tâche en tenant le verrou** — le worker déverrouille avant d'appeler la
  tâche, sinon un seul worker peut travailler à la fois (les autres restent bloqués sur le mutex).

---

## Étapes d'implémentation

### 1. Le type de tâche stocké dans la queue

`std::function<void()>` — un callable type-erased sans argument ni valeur de retour. Le "sans valeur
de retour" est correct même si `submit()` accepte des callables à valeur de retour : la valeur de
retour part dans la `future` via le `packaged_task`/`promise`, la tâche elle-même (une fois enveloppée)
n'a plus besoin de rien retourner directement.

### 2. Les membres partagés

- `std::queue<std::function<void()>> _tasks;`
- `std::mutex _mutex;` (protège `_tasks` et `_stop`)
- `std::condition_variable _condition;`
- `std::vector<std::thread> _workers;`
- `bool _stop = false;`

### 3. La boucle d'un worker

Chaque thread de `_workers` exécute une fonction membre (ex: `workerLoop()`) en boucle infinie :

1. Verrouille `_mutex` (via `std::unique_lock`, pas `std::lock_guard` — nécessaire pour
   `condition_variable::wait`).
2. `_condition.wait(lock, predicate)` où le prédicat est `[this] { return _stop || !_tasks.empty(); }`
   — attend efficacement (pas de busy-wait) tant que la queue est vide et qu'on n'a pas demandé
   l'arrêt.
3. Si `_stop && _tasks.empty()` → sortir de la boucle (fin du thread).
4. Sinon : extraire une tâche de `_tasks` (`std::move`, puis `pop()`).
5. **Déverrouiller** (`lock.unlock()`) avant d'exécuter la tâche — sinon aucun autre worker ne peut
   progresser pendant l'exécution.
6. Exécuter la tâche (`task();`).
7. Reboucler à l'étape 1.

### 4. Constructeur / destructeur

- **Constructeur** : lance N `std::thread(&ThreadPool::workerLoop, this)`, stockés dans `_workers`.
- **Destructeur** :
  1. Verrouille `_mutex`, passe `_stop = true`, déverrouille.
  2. `_condition.notify_all()` — réveille TOUS les workers potentiellement endormis (pas
     `notify_one()`, sinon certains resteraient bloqués indéfiniment).
  3. `.join()` chaque thread de `_workers` (attend qu'ils terminent réellement avant que le pool ne
     soit détruit — sinon des threads pourraient continuer à tourner sur un objet déjà détruit).

### 5. `submit()` — la méthode générique

Signature visée (approximative, syntaxe exacte à affiner) :

```cpp
template <typename F, typename... Args>
auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;
```

Logique interne :

1. Déduire `ReturnType = std::invoke_result_t<F, Args...>`.
2. Créer un `std::packaged_task<ReturnType()>` à partir de `f` et `args` liés ensemble (ex: via
   `std::bind` ou une lambda de capture) — enveloppé dans un `std::shared_ptr` pour le rendre copiable
   (voir le piège plus haut).
3. Récupérer sa `future` (`task->get_future()`) **avant** de le pousser dans la queue — une fois
   déplacé dans la queue, tu n'y as plus accès directement.
4. Pousser dans `_tasks` une lambda `[task] { (*task)(); }` (capture le `shared_ptr` par valeur — donc
   copiable, donc compatible avec `std::function<void()>`).
5. `_condition.notify_one()` — réveille un seul worker, une seule tâche a été ajoutée.
6. Retourner la `future` récupérée à l'étape 3.

### 6. Intégration dans `World` (une fois le pool fonctionnel isolément)

Pas de changement au pool pour ça — juste la façon dont `World` l'utilise :

- `World` possède un `ThreadPool _pool;` (ou une référence si le pool est partagé/possédé par `Game`).
- `scheduleChunkGeneration`/`scheduleChunkMeshing` se découpent comme déjà discuté :
  - Partie synchrone (thread principal) : tout ce qui touche `_chunks` (insert/erase) ou `Renderer`
    (create/destroy mesh/texture), et les transitions d'état (`setState`).
  - Partie job (`pool.submit(...)`) : `TerrainGenerator::generate`, `ChunkMesher::getMeshData`, le
    calcul du tableau de tile index (à extraire de `ChunkDataTexture::build`, qui mélange
    actuellement calcul CPU et appel `createTexture3D` — à séparer en deux fonctions).
- Dans `World::update()` : après avoir soumis un batch de jobs (génération ou meshing), collecter les
  `std::future<...>` correspondantes dans un `std::vector`, puis boucler dessus avec `.get()` pour
  attendre que tout le batch soit terminé avant de faire la partie synchrone qui en dépend
  (`setState`, `createMesh`, `createTexture3D`).

---

## Points de vigilance déjà identifiés dans les discussions précédentes

- `Renderer::createMesh`/`createTexture3D`/`destroyMesh`/`destroyTexture3D` doivent **rester appelés
  uniquement depuis le thread principal** — `Renderer` n'est pas thread-safe (`_meshes` est une
  `unordered_map` non protégée, `createTexture3D` utilise un command pool/queue partagés qui ne
  supportent pas la soumission concurrente).
- Les jobs de génération/meshing ne doivent **jamais** recevoir de `Chunk&`/`Chunk*` vers un chunk
  encore en cours d'écriture ailleurs — la génération d'un chunk n'a besoin d'aucune donnée
  extérieure, et le meshing d'un chunk ne doit démarrer qu'une fois lui-même et ses 6 voisins passés
  l'état `NOT_GENERATED` (déjà vérifié dans `World::update()`).
- Le culling inter-chunk (les colonnes élargies de 18 bits dans `ChunkMesher`) suppose que les
  chunks voisins ne sont plus modifiés pendant qu'on les lit — cohérent avec le point précédent tant
  que l'édition de blocs (future feature) est elle-même bien synchronisée avec le pool.
