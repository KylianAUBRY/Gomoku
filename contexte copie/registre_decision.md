* **ADR-001** : Choix du langage. -> *[C++]* (Performances optimales pour le calcul de l'arbre Min-Max).
* **ADR-002** : Choix de la bibliothèque graphique principale. -> *[SFML]* (Outil de validation pour la vue 2D obligatoire).
* **ADR-003** : Séparation Logique/Graphisme. -> *[Architecture MVC]* Isolation totale du code métier pour éviter les conflits de contexte entre SFML et l'outil 3D.
* **ADR-004** : Optimisation du Min-Max. -> *[Élagage Alpha-Beta]* (Requis pour garantir une profondeur de 10 sous la limite de 0.5 seconde).
* **ADR-005** : Implémentation du Bonus 3D. -> *[Raylib]* Instancié uniquement via un argument au lancement (`--3d`). Le développement de ce module est bloqué tant que le noyau et l'ADR-002 ne sont pas validés à 100%.