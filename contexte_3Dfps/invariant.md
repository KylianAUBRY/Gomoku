**Contraintes Non-Negociables (Mode FPS 3D)**

* **Coexistence** : Le mode 2D SFML doit rester 100% fonctionnel. Aucun fichier dans `src/front/` ne doit etre modifie pour ajouter le mode 3D.
* **Moteur IA partage** : Le mode 3D utilise exactement les memes classes `Gomoku`, `BitBoard`, `GameState`, `Rules` que le mode 2D. Aucune duplication du code moteur.
* **Performance IA** : Meme contrainte que le mode 2D — temps de reflexion moyen < 0.5 seconde, profondeur >= 10.
* **Stabilite** : Aucun crash, meme en cas de raycast rate ou d'actions rapides. Le rendu 3D ne doit jamais bloquer le thread IA.
* **Camera fixe** : Le joueur ne peut PAS se deplacer. Seule la rotation de la camera (look around) est autorisee.
* **Regles identiques** : Captures, double-trois, endgame capture — toutes les regles du sujet s'appliquent a l'identique.
* **Chronometre** : Obligatoirement affiche dans le HUD 3D (condition de validation du sujet).
* **Build** : Le Makefile existant doit etre etendu (pas remplace). Le mode 3D se compile avec la meme commande ou un flag additionnel. Pas de relink.
* **Lancement** : Le mode 3D s'active via un argument en ligne de commande (ex: `./Gomoku --fps`). Par defaut, le mode 2D se lance.
* **Pas de conflit de contexte graphique** : SFML et Raylib ne doivent jamais etre initialises simultanement dans le meme processus.
