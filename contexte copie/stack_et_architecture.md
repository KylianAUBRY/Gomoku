**Stack Technique**
* [cite_start]**Langage** : C++[cite: 32].
* **Noyau Logique (Core)** : C++ standard, strictement agnostique de toute bibliothèque graphique.
* [cite_start]**Vue 2D (Obligatoire)** : SFML[cite: 32].
* [cite_start]**Vue 3D (Bonus)** : Raylib[cite: 32].

**Architecture Logicielle**
* **Design Pattern** : Modèle-Vue-Contrôleur (MVC) strict. Le noyau logique interagit avec des interfaces d'affichage abstraites. L'implémentation spécifique (SFML ou Raylib) est injectée au lancement.
* [cite_start]**Moteur IA** : Algorithme Min-Max générant un arbre de résolutions possibles[cite: 50, 51].
* [cite_start]**Évaluation** : Implémentation d'une fonction heuristique rapide et précise pour évaluer les nœuds terminaux[cite: 51, 52].
* [cite_start]**Débogage** : Implémentation d'un module affichant le raisonnement de l'IA en temps réel[cite: 60].