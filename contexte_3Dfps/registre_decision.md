* **ADR-3D-001** : Bibliotheque 3D. -> *[Raylib]* Leger, API simple en C, pas de dependances systeme lourdes. Compatible avec le build C++ existant. Conforme a l'ADR-005 du contexte principal.

* **ADR-3D-002** : Isolation SFML / Raylib. -> *[Branchement dans main.cpp]* Un seul contexte graphique actif par execution. Le flag `--fps` determine quel module de vue est instancie. SFML et Raylib ne coexistent jamais dans le meme processus.

* **ADR-3D-003** : Pas d'interface abstraite. -> *[Duplication du contrat, pas du code]* L'architecture 2D existante n'utilise pas de classes abstraites (pas de virtual). Plutot que de refactorer tout le mode 2D pour introduire une interface `IGameView`, le mode 3D replique le meme contrat (memes appels a `GameState` et `Gomoku`) dans ses propres fichiers. Le moteur reste intouche.

* **ADR-3D-004** : Camera FPS fixe. -> *[Rotation seule, position lockee]* Le joueur ne bouge pas. La position de la camera est calculee une fois au lancement pour cadrer le goban entier. Seuls le yaw et le pitch sont libres (via souris). Raison : simplifier le gameplay, eviter les problemes de collision/navigation, garder le focus sur le jeu.

* **ADR-3D-005** : Placement des pierres par raycast. -> *[GetMouseRay + plan intersection]* Le clic gauche lance un rayon depuis le centre de l'ecran. L'intersection avec le plan du goban donne les coordonnees 3D, converties en indices (row, col) par snap a l'intersection la plus proche. Seuil de tolerance pour eviter les clics hors grille.

* **ADR-3D-006** : Dossier dedie `src/front_3d/`. -> *[Separation physique]* Aucun fichier du mode 2D (`src/front/`) n'est touche. Le mode 3D vit dans son propre dossier. Le Makefile ajoute ces sources uniquement si Raylib est disponible.

* **ADR-3D-007** : Thread IA. -> *[Thread separe, meme pattern que le 2D]* Le calcul minimax est lance dans un `std::thread` ou `std::async`. Le resultat est ecrit dans `GameState`. Le thread de rendu Raylib lit l'etat et affiche un indicateur de "reflexion IA" en attendant.

* **ADR-3D-008** : HUD en overlay 2D. -> *[DrawText Raylib apres EndMode3D]* Le chronometre, les captures et l'historique sont dessines en 2D par-dessus la scene 3D, apres `EndMode3D()` et avant `EndDrawing()`. Pas besoin de texte 3D dans la scene.
