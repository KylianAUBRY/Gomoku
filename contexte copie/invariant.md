**Points de Défaillance Critiques (Validation du projet)**

* [cite_start]**Performance** : Temps de réflexion moyen de l'IA strictement inférieur à 0,5 seconde[cite: 39].
* [cite_start]**Profondeur d'arbre** : L'algorithme Min-Max doit explorer au minimum 10 niveaux[cite: 55].
* **Stabilité** : Aucun crash (même en cas de manque de mémoire) ou arrêt inattendu. [cite_start]Sanction : Note de 0[cite: 33, 34].
* [cite_start]**Build** : Makefile requis avec les règles `$(NAME)`, `all`, `clean`, `fclean` et `re`[cite: 35, 37]. [cite_start]Interdiction formelle de relink[cite: 35].
* [cite_start]**Livrable** : L'exécutable doit être nommé `Gomoku`[cite: 44].
* [cite_start]**Interface** : L'absence de chronomètre entraîne la non-validation du projet[cite: 68].
* [cite_start]**Condition d'évaluation du Bonus** : La partie bonus ne sera évaluée que si la partie obligatoire est parfaite et fonctionnelle à 100%[cite: 78, 79].

**Risques d'Architecture**
* **Fuites Mémoire** : La destruction des contextes graphiques doit être rigoureuse.
* **Blocage Thread** : Le calcul IA ne doit pas geler l'affichage du timer.