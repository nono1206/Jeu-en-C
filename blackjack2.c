#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_JOUEURS 3
#define MAX_CARTES 12
#define BANKROLL_INITIAL 1000

// ============================================
// ÉNUMÉRATIONS - Définitions des types de base
// ============================================

// Les 4 couleurs d'un jeu de cartes
typedef enum { COEUR, CARREAU, TREFLE, PIQUE } Couleur;

// Les 13 rangs d'un jeu de cartes (de 2 à As)
typedef enum { 
    DEUX=2, TROIS, QUATRE, CINQ, SIX, SEPT, HUIT, NEUF, DIX,
    VALET=11, DAME=12, ROI=13, AS=14
} Rang;

// Les différents états possibles du jeu
typedef enum {
    ETAT_SELECTION_JOUEURS,  // Choix du nombre de joueurs
    ETAT_PARI,               // Phase où les joueurs placent leurs mises
    ETAT_TOUR_JOUEUR,        // Phase où les joueurs jouent leur main
    ETAT_TOUR_CROUPIER,      // Phase où le croupier joue
    ETAT_ROUND_TERMINE,      // Fin d'un round, calcul des gains
    ETAT_JEU_TERMINE         // Jeu terminé
} EtatJeu;

// ============================================
// STRUCTURES - Définitions des objets du jeu
// ============================================

// Représente une carte avec sa couleur et son rang
typedef struct {
    Couleur couleur;
    Rang rang;
} Carte;

// Représente une main de cartes (joueur ou croupier)
typedef struct {
    Carte* cartes[MAX_CARTES];  // Tableau de pointeurs vers les cartes
    int nb_cartes;               // Nombre de cartes dans la main
    int valeur;                  // Valeur totale de la main (0-21+)
} Main;

// Représente un joueur avec toutes ses données
typedef struct {
    char nom[50];        // Nom du joueur
    Main* main;          // Pointeur vers sa main de cartes
    int bankroll;        // Argent disponible
    int pari;            // Mise actuelle du round
    int actif;           // 1 si le joueur joue ce round, 0 sinon
    int a_accepte;       // 1 si le joueur a décidé de ne plus tirer
    int est_busted;      // 1 si le joueur a dépassé 21
} Joueur;

// Gère tous les joueurs de la partie
typedef struct {
    Joueur joueurs[MAX_JOUEURS];  // Tableau des joueurs
    int nb_joueurs;                // Nombre total de joueurs
    int index_joueur_actuel;       // Index du joueur dont c'est le tour
} GestionnaireJoueurs;

// Représente le sabot (paquet de cartes mélangées)
typedef struct {
    Carte cartes[364];           // 7 jeux de 52 cartes = 364 cartes
    int index_prochaine_carte;   // Index de la prochaine carte à tirer
    int nb_cartes;               // Nombre total de cartes dans le sabot
} Sabot;

// Structure principale du jeu
typedef struct {
    EtatJeu etat;                          // État actuel du jeu
    GestionnaireJoueurs* gestionnaire_joueurs;  // Gère les joueurs
    Main* main_croupier;                   // Main du croupier
    Sabot* sabot;                          // Sabot de cartes
    int numero_round;                      // Numéro du round actuel
} Jeu;

// ============================================
// FONCTIONS UTILITAIRES POUR LES CARTES
// ============================================

/**
 * Retourne la valeur en points d'une carte
 * - As = 11 (sera ajusté à 1 si nécessaire)
 * - Figures (V, D, R) = 10
 * - Autres cartes = leur valeur numérique
 */
int valeur_carte(Carte* carte) {
    if (carte->rang == AS) return 11;
    if (carte->rang >= 10) return 10;
    return carte->rang;
}

/**
 * Convertit un rang de carte en chaîne de caractères
 * Exemple: VALET -> "V", AS -> "A", DIX -> "10"
 */
const char* rang_vers_string(Rang rang) {
    switch(rang) {
        case AS: return "A";
        case DEUX: return "2";
        case TROIS: return "3";
        case QUATRE: return "4";
        case CINQ: return "5";
        case SIX: return "6";
        case SEPT: return "7";
        case HUIT: return "8";
        case NEUF: return "9";
        case DIX: return "10";
        case VALET: return "V";
        case DAME: return "D";
        case ROI: return "R";
        default: return "?";
    }
}

/**
 * Convertit une couleur en symbole Unicode
 * Affiche les symboles ♥ ♦ ♣ ♠
 */
const char* couleur_vers_string(Couleur couleur) {
    switch(couleur) {
        case COEUR: return "♥";
        case CARREAU: return "♦";
        case TREFLE: return "♣";
        case PIQUE: return "♠";
        default: return "?";
    }
}

/**
 * Affiche une carte au format [RangCouleur]
 * Exemple: [A♠] pour l'As de Pique
 */
void afficher_carte(Carte* carte) {
    printf("[%s%s]", rang_vers_string(carte->rang), couleur_vers_string(carte->couleur));
}

// ============================================
// GESTION DES MAINS
// ============================================

/**
 * Crée une nouvelle main vide
 * Alloue la mémoire et initialise tous les champs à 0/NULL
 */
Main* creer_main() {
    Main* main = malloc(sizeof(Main));
    main->nb_cartes = 0;
    main->valeur = 0;
    
    // Initialise tous les pointeurs de cartes à NULL
    for (int i = 0; i < MAX_CARTES; i++) { 
        main->cartes[i] = NULL;
    }
    return main;
}

/**
 * Libère la mémoire allouée pour une main
 */
void detruire_main(Main* main) {
    if (main) free(main);
}

/**
 * Calcule la valeur totale d'une main
 * Gère automatiquement les As (11 ou 1 selon le total)
 * - As compte 11 par défaut
 * - Si le total dépasse 21, les As comptent 1
 */
int calculer_valeur_main(Main* main) {
    if (!main || main->nb_cartes == 0) return 0;
    
    int valeur = 0;
    int as = 0;  // Compte le nombre d'As comptés comme 11
    
    // Première passe: additionne toutes les valeurs
    for (int i = 0; i < main->nb_cartes; i++) {
        if (main->cartes[i]) {
            int val = valeur_carte(main->cartes[i]);
            
            if (main->cartes[i]->rang == AS) {
                as++;
                valeur += 11;
            } else {
                valeur += val;
            }
        }
    }
    
    // Ajuste les As de 11 à 1 si nécessaire pour ne pas dépasser 21
    while (valeur > 21 && as > 0) {
        valeur -= 10;  // Transformer un As de 11 en 1
        as--;
    }
    
    main->valeur = valeur;
    return valeur;
}

/**
 * Ajoute une carte à une main et recalcule sa valeur
 */
void ajouter_carte_main(Main* main, Carte* carte) {
    if (main->nb_cartes < MAX_CARTES) {
        main->cartes[main->nb_cartes++] = carte;
        calculer_valeur_main(main);
    }
}

/**
 * Vérifie si une main est un Blackjack
 * Un Blackjack = 21 points avec exactement 2 cartes
 */
int est_blackjack(Main* main) {
    return (main->nb_cartes == 2 && main->valeur == 21);
}

/**
 * Vérifie si une main a dépassé 21 (bust)
 */
int est_busted(Main* main) {
    return (main->valeur > 21);
}

/**
 * Vérifie si le joueur peut scinder sa main (split)
 * Possible seulement avec 2 cartes de même rang
 * (Fonctionnalité non implémentée dans ce code)
 */
int peut_scinder_main(Main* main) {
    return (main->nb_cartes == 2 && 
            main->cartes[0]->rang == main->cartes[1]->rang);
}

/**
 * Affiche une main de cartes
 * @param cacher_deuxieme: si 1, cache la 2ème carte (pour le croupier)
 */
void afficher_main(Main* main, int cacher_deuxieme) {
    if (main->nb_cartes == 0) return;
    
    // Affiche chaque carte
    for (int i = 0; i < main->nb_cartes; i++) {
        if (cacher_deuxieme && i == 1) {
            printf("[??]");  // Cache la carte du croupier
        } else {
            afficher_carte(main->cartes[i]);
        }
        printf(" ");
    }
    
    // Affiche la valeur totale (sauf si carte cachée)
    if (!cacher_deuxieme) {
        printf("(Valeur: %d)", main->valeur);
    }
}

/**
 * Réinitialise une main pour un nouveau round
 * Vide la main de toutes ses cartes
 */
void reinitialiser_main(Main* main) {
    main->nb_cartes = 0;
    main->valeur = 0;
    
    for (int i = 0; i < MAX_CARTES; i++) {
        main->cartes[i] = NULL;
    }
}

// ============================================
// GESTION DU SABOT (paquet de cartes)
// ============================================

/**
 * Crée un nouveau sabot avec 7 jeux de 52 cartes
 * Total: 364 cartes (7 × 4 couleurs × 13 rangs)
 */
Sabot* creer_sabot() {
    Sabot* sabot = malloc(sizeof(Sabot));
    sabot->index_prochaine_carte = 0;
    sabot->nb_cartes = 0;
    
    // Crée 7 jeux de cartes complets
    for (int d = 0; d < 7; d++) {
        // Pour chaque couleur
        for (Couleur c = COEUR; c <= PIQUE; c++) {
            // Pour chaque rang (2 à As)
            for (Rang r = DEUX; r <= AS; r++) {
                sabot->cartes[sabot->nb_cartes].couleur = c;
                sabot->cartes[sabot->nb_cartes].rang = r;
                sabot->nb_cartes++;
            }
        }
    }
    
    return sabot;
}

/**
 * Libère la mémoire d'un sabot
 */
void detruire_sabot(Sabot* sabot) {
    if (sabot) free(sabot);
}

/**
 * Mélange toutes les cartes du sabot de façon aléatoire
 * Utilise l'algorithme de Fisher-Yates
 */
void melanger_sabot(Sabot* sabot) {
    srand(time(NULL));  // Initialise le générateur aléatoire
    
    // Pour chaque carte, l'échange avec une carte aléatoire
    for (int i = 0; i < sabot->nb_cartes; i++) {
        int j = rand() % sabot->nb_cartes;
        
        // Échange les cartes i et j
        Carte temp = sabot->cartes[i];
        sabot->cartes[i] = sabot->cartes[j];
        sabot->cartes[j] = temp;
    }
    
    sabot->index_prochaine_carte = 0;
}

/**
 * Tire la prochaine carte du sabot
 * Retourne NULL s'il n'y a plus de cartes
 */
Carte* tirer_carte(Sabot* sabot) {
    if (sabot->index_prochaine_carte >= sabot->nb_cartes) return NULL;
    
    return &sabot->cartes[sabot->index_prochaine_carte++];
}

// ============================================
// GESTION DES JOUEURS
// ============================================

/**
 * Crée un nouveau joueur avec un nom et un bankroll initial
 */
Joueur creer_joueur(const char* nom) {
    Joueur j;
    
    strcpy(j.nom, nom);
    j.main = creer_main();
    j.bankroll = BANKROLL_INITIAL;  // 1000$ de départ
    j.pari = 0;
    j.actif = 1;
    j.a_accepte = 0;
    j.est_busted = 0;
    return j;
}

/**
 * Libère la mémoire d'un joueur
 */
void detruire_joueur(Joueur* j) {
    if (j && j->main) detruire_main(j->main);
}

/**
 * Enregistre le pari d'un joueur
 * Déduit le montant du bankroll
 */
void placer_pari(Joueur* j, int montant) {
    if (montant > 0 && montant <= j->bankroll) {
        j->pari = montant;
        j->bankroll -= montant;  // Retire l'argent misé
        printf("%s mise: $%d\n", j->nom, montant);
    }
}

/**
 * Prépare un joueur pour un nouveau round
 * Vide sa main, réinitialise son statut
 */
void reinitialiser_joueur_pour_round(Joueur* j) {
    reinitialiser_main(j->main);
    j->pari = 0;
    j->actif = 1;
    j->a_accepte = 0;
    j->est_busted = 0;
}

// ============================================
// GESTIONNAIRE DE JOUEURS
// ============================================

/**
 * Crée un gestionnaire avec un nombre donné de joueurs
 * Initialise chaque joueur avec un nom générique
 */
GestionnaireJoueurs* creer_gestionnaire_joueurs(int nb) {
    GestionnaireJoueurs* gj = malloc(sizeof(GestionnaireJoueurs));
    gj->nb_joueurs = nb;
    gj->index_joueur_actuel = 0;
    
    // Crée chaque joueur
    for (int i = 0; i < nb; i++) {
        char nom[20];
        snprintf(nom, sizeof(nom), "Joueur %d", i + 1);
        gj->joueurs[i] = creer_joueur(nom);
    }
    
    return gj;
}

/**
 * Libère la mémoire du gestionnaire et de tous les joueurs
 */
void detruire_gestionnaire_joueurs(GestionnaireJoueurs* gj) {
    if (gj) {
        // Détruit chaque joueur
        for (int i = 0; i < gj->nb_joueurs; i++) {
            detruire_joueur(&gj->joueurs[i]);
        }
        free(gj);
    }
}

/**
 * Retourne le joueur dont c'est actuellement le tour
 */
Joueur* obtenir_joueur_actuel(GestionnaireJoueurs* gj) {
    if (gj && gj->index_joueur_actuel < gj->nb_joueurs) {
        return &gj->joueurs[gj->index_joueur_actuel];
    }
    return NULL;
}

/**
 * Passe au joueur suivant
 * Retourne 1 s'il reste des joueurs, 0 sinon
 */
int joueur_suivant(GestionnaireJoueurs* gj) {
    gj->index_joueur_actuel++;
    return gj->index_joueur_actuel < gj->nb_joueurs;
}

/**
 * Vérifie si tous les joueurs sont ruinés (bankroll = 0)
 */
int tous_joueurs_ruines(GestionnaireJoueurs* gj) {
    for (int i = 0; i < gj->nb_joueurs; i++) {
        if (gj->joueurs[i].bankroll > 0) return 0;
    }
    return 1;
}

// ============================================
// LOGIQUE PRINCIPALE DU JEU
// ============================================

/**
 * Crée et initialise une nouvelle partie de Blackjack
 * Prépare le sabot et les structures de données
 */
Jeu* creer_jeu() {
    Jeu* j = malloc(sizeof(Jeu));
    j->etat = ETAT_SELECTION_JOUEURS;
    j->main_croupier = creer_main();
    j->sabot = creer_sabot();
    melanger_sabot(j->sabot);
    j->numero_round = 0;
    j->gestionnaire_joueurs = NULL;
    return j;
}

/**
 * Libère toute la mémoire du jeu
 */
void detruire_jeu(Jeu* j) {
    if (j) {
        if (j->main_croupier) detruire_main(j->main_croupier);
        if (j->sabot) detruire_sabot(j->sabot);
        if (j->gestionnaire_joueurs) detruire_gestionnaire_joueurs(j->gestionnaire_joueurs);
        free(j);
    }
}

/**
 * Démarre le jeu avec un nombre spécifié de joueurs
 */
void demarrer_jeu_avec_joueurs(Jeu* j, int nb) {
    if (j->gestionnaire_joueurs) detruire_gestionnaire_joueurs(j->gestionnaire_joueurs);
    j->gestionnaire_joueurs = creer_gestionnaire_joueurs(nb);
    j->etat = ETAT_PARI;
    j->numero_round = 1;
}

/**
 * Distribue 2 cartes à chaque joueur et au croupier
 * Marque le début du round de jeu
 */
void distribuer_cartes_initiales(Jeu* j) {
    // Donne 2 cartes à chaque joueur qui a parié
    for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
        Joueur* p = &j->gestionnaire_joueurs->joueurs[i];
        
        if (p->actif && p->pari > 0) {
            ajouter_carte_main(p->main, tirer_carte(j->sabot));
            ajouter_carte_main(p->main, tirer_carte(j->sabot));
        }
    }
    
    // Donne 2 cartes au croupier
    ajouter_carte_main(j->main_croupier, tirer_carte(j->sabot));
    ajouter_carte_main(j->main_croupier, tirer_carte(j->sabot));
    
    j->etat = ETAT_TOUR_JOUEUR;
    printf("\n📋 Cartes distribuées!\n\n");
}

/**
 * Le joueur actuel tire une carte
 * Vérifie automatiquement s'il fait bust
 */
void joueur_tire(Jeu* j) {
    Joueur* j_act = obtenir_joueur_actuel(j->gestionnaire_joueurs);
    
    if (j_act) {
        ajouter_carte_main(j_act->main, tirer_carte(j->sabot));
        printf("%s tire. Nouvelle valeur: %d\n", j_act->nom, j_act->main->valeur);
        
        // Vérifie si le joueur a dépassé 21
        if (est_busted(j_act->main)) {
            j_act->est_busted = 1;
            printf("💥 %s BUST!\n\n", j_act->nom);
            joueur_suivant(j->gestionnaire_joueurs);
        }
    }
}

/**
 * Le joueur actuel décide de ne plus tirer (stand)
 */
void joueur_accepte(Jeu* j) {
    Joueur* j_act = obtenir_joueur_actuel(j->gestionnaire_joueurs);
    
    if (j_act) {
        j_act->a_accepte = 1;
        printf("%s accepte avec %d\n\n", j_act->nom, j_act->main->valeur);
        joueur_suivant(j->gestionnaire_joueurs);
    }
}

/**
 * Gère le tour du croupier
 * Le croupier tire jusqu'à avoir au moins 17
 */
void jeu_croupier(Jeu* j) {
    printf("\n🎯 Tour du croupier...\n");
    printf("Le croupier révèle: ");
    afficher_main(j->main_croupier, 0);
    printf("\n\n");
    
    // Règle du croupier: tire jusqu'à 17 minimum
    while (j->main_croupier->valeur < 17) {
        ajouter_carte_main(j->main_croupier, tirer_carte(j->sabot));
        printf("Le croupier tire: %d\n", j->main_croupier->valeur);
    }
    
    if (est_busted(j->main_croupier)) {
        printf("💥 Croupier BUST! (%d)\n\n", j->main_croupier->valeur);
    } else {
        printf("Le croupier accepte à %d\n\n", j->main_croupier->valeur);
    }
    
    j->etat = ETAT_ROUND_TERMINE;
}

/**
 * Évalue les résultats du round et distribue les gains
 * Compare chaque main de joueur avec celle du croupier
 */
void evaluer_round(Jeu* j) {
    printf("╔══ RESULTATS ROUND %d ══╗\n", j->numero_round);
    printf("Croupier: %d points%s\n\n", 
           j->main_croupier->valeur,
           est_busted(j->main_croupier) ? " (BUST)" : "");
    
    int val_croupier = j->main_croupier->valeur;
    int croupier_bust = est_busted(j->main_croupier);
    
    // Évalue chaque joueur
    for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
        Joueur* p = &j->gestionnaire_joueurs->joueurs[i];
        
        if (p->pari == 0) continue;  // Ignore les joueurs sans pari
        
        int val_joueur = p->main->valeur;
        int joueur_bust = p->est_busted;
        int joueur_bj = est_blackjack(p->main);
        int croupier_bj = est_blackjack(j->main_croupier);
        
        printf("%s: %d", p->nom, val_joueur);
        if (joueur_bj) printf(" (BLACKJACK!)");
        if (joueur_bust) printf(" (BUST)");
        printf(" | Pari: $%d | ", p->pari);
        
        // Calcul des gains selon les règles du Blackjack
        if (joueur_bj && !croupier_bj) {
            // Blackjack paie 3:2
            int gains = (p->pari * 3) / 2;
            p->bankroll += p->pari + gains;
            printf("GAGNE 3:2 +$%d\n", gains);
        } else if (joueur_bust) {
            // Joueur bust = perd sa mise
            printf("PERD -$%d\n", p->pari);
        } else if (croupier_bust) {
            // Croupier bust = joueur gagne 1:1
            p->bankroll += p->pari * 2;
            printf("GAGNE +$%d\n", p->pari);
        } else if (croupier_bj && !joueur_bj) {
            // Croupier blackjack = joueur perd
            printf("PERD -$%d\n", p->pari);
        } else if (val_joueur > val_croupier) {
            // Joueur a une meilleure main
            p->bankroll += p->pari * 2;
            printf("GAGNE +$%d\n", p->pari);
        } else if (val_joueur < val_croupier) {
            // Croupier a une meilleure main
            printf("PERD -$%d\n", p->pari);
        } else {
            // Égalité = remboursement
            p->bankroll += p->pari;
            printf("EGALITE =$0\n");
        }
        
        printf("Bankroll: $%d\n\n", p->bankroll);
    }
}

/**
 * Prépare un nouveau round
 * Réinitialise les mains et vérifie le sabot
 */
void demarrer_nouveau_round(Jeu* j) {
    j->numero_round++;
    
    // Réinitialise tous les joueurs
    for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
        reinitialiser_joueur_pour_round(&j->gestionnaire_joueurs->joueurs[i]);
    }
    
    reinitialiser_main(j->main_croupier);
    j->gestionnaire_joueurs->index_joueur_actuel = 0;
    
    // Remélange si moins de 20 cartes restantes
    if (j->sabot->index_prochaine_carte > j->sabot->nb_cartes - 20) {
        melanger_sabot(j->sabot);
        printf("🔀 Sabot remélangé\n\n");
    }
    
    j->etat = ETAT_PARI;
}

/**
 * Affiche l'état actuel de la table de jeu
 * Montre la main du croupier et de chaque joueur
 */
void afficher_jeu(Jeu* j) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    
    // Affiche pendant la phase de jeu
    if (j->etat == ETAT_TOUR_JOUEUR || 
        j->etat == ETAT_TOUR_CROUPIER ||
        j->etat == ETAT_ROUND_TERMINE) {
        
        printf("║ CROUPIER: ");
        // Cache la 2ème carte pendant le tour des joueurs
        afficher_main(j->main_croupier, 
                    (j->etat == ETAT_TOUR_JOUEUR) ? 1 : 0);
        printf("\n");
        printf("╠══════════════════════════════════════════════════╣\n");
        
        // Affiche tous les joueurs
        for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
            Joueur* p = &j->gestionnaire_joueurs->joueurs[i];
            printf("║ %s: ", p->nom);
            afficher_main(p->main, 0);
            printf("\n");
        }
    }
    
    printf("╚══════════════════════════════════════════════════╝\n");
}

// ============================================
// FONCTION PRINCIPALE - BOUCLE DE JEU
// ============================================

/**
 * Point d'entrée du programme
 * Gère la boucle principale du jeu et les différentes phases
 */
int main() {
    printf("🎰 CASINO BLACKJACK\n\n");
    
    // Création et initialisation du jeu
    Jeu* jeu = creer_jeu();
    int quitter = 0;
    
    // Boucle principale du jeu
    while (!quitter) {
        
        // ÉTAT 1: Sélection du nombre de joueurs
        if (jeu->etat == ETAT_SELECTION_JOUEURS) {
            printf("Combien de joueurs? (1-3): ");
            int nb;
            scanf("%d", &nb);
            
            // Validation et démarrage du jeu avec le nombre de joueurs choisi
            if (nb >= 1 && nb <= 3) {
                demarrer_jeu_avec_joueurs(jeu, nb);
            }
        }
        
        // ÉTAT 2: Phase de pari - chaque joueur place sa mise
        else if (jeu->etat == ETAT_PARI) {
            printf("\n╔═══ PHASE DE PARI ═══╗\n");
            int tous_ont_parie = 1;
            
            // Parcours de tous les joueurs pour collecter leurs paris
            for (int i = 0; i < jeu->gestionnaire_joueurs->nb_joueurs; i++) {
                Joueur* p = &jeu->gestionnaire_joueurs->joueurs[i];
                printf("\n%s - Bankroll: $%d\n", p->nom, p->bankroll);
                
                int pari = 0;
                printf("Entrez votre pari: $");
                scanf("%d", &pari);
                
                // Validation du pari (doit être positif et <= bankroll)
                if (pari > 0 && pari <= p->bankroll) {
                    placer_pari(p, pari);
                    p->actif = 1;
                } else {
                    printf("Pari invalide!\n");
                    i--; // Redemander le pari au même joueur
                    continue;
                }
            }
            
            // Distribution des cartes initiales (2 par joueur + 2 pour le croupier)
            distribuer_cartes_initiales(jeu);
        }
        
        // ÉTAT 3: Tour des joueurs - chaque joueur joue à son tour
        else if (jeu->etat == ETAT_TOUR_JOUEUR) {
            afficher_jeu(jeu);
            
            Joueur* p = obtenir_joueur_actuel(jeu->gestionnaire_joueurs);
            
            // Si le joueur actuel a fini de jouer (accepté ou bust), passer au suivant
            if (!p || p->a_accepte || p->est_busted) {
                // Si plus de joueurs, passer au tour du croupier
                if (!joueur_suivant(jeu->gestionnaire_joueurs)) {
                    jeu->etat = ETAT_TOUR_CROUPIER;
                }
            } else {
                // Demander l'action au joueur actuel
                printf("\nTour de %s (Valeur: %d)\n", p->nom, p->main->valeur);
                printf("[1] Tirer  [2] Accepter: ");
                
                int choix;
                scanf("%d", &choix);
                
                // Exécuter l'action choisie
                if (choix == 1) {
                    joueur_tire(jeu);
                } else {
                    joueur_accepte(jeu);
                }
            }
        }
        
        // ÉTAT 4: Tour du croupier - le croupier joue selon les règles (tire jusqu'à 17)
        else if (jeu->etat == ETAT_TOUR_CROUPIER) {
            afficher_jeu(jeu);
            jeu_croupier(jeu);
        }
        
        // ÉTAT 5: Fin du round - évaluation des résultats et paiement des gains
        else if (jeu->etat == ETAT_ROUND_TERMINE) {
            evaluer_round(jeu);
            
            // Vérifier si tous les joueurs sont ruinés (bankroll = 0)
            if (tous_joueurs_ruines(jeu->gestionnaire_joueurs)) {
                jeu->etat = ETAT_JEU_TERMINE;
            } else {
                // Demander si les joueurs veulent continuer
                printf("Jouer un nouveau round? (1=Oui, 0=Non): ");
                int continuer;
                scanf("%d", &continuer);
                
                if (continuer == 1) {
                    demarrer_nouveau_round(jeu);
                } else {
                    quitter = 1;
                }
            }
        }
        
        // ÉTAT 6: Fin du jeu - affichage du classement final
        else if (jeu->etat == ETAT_JEU_TERMINE) {
            printf("\n🎭 JEU TERMINE - SANS ARGENT!\n");
            printf("Classement final:\n");
            
            // Afficher le bankroll final de chaque joueur
            for (int i = 0; i < jeu->gestionnaire_joueurs->nb_joueurs; i++) {
                Joueur* p = &jeu->gestionnaire_joueurs->joueurs[i];
                printf("%s: $%d\n", p->nom, p->bankroll);
            }
            quitter = 1;
        }
    }
    
    // Libération de toute la mémoire allouée
    detruire_jeu(jeu);
    printf("\n👋 Merci d'avoir joué!\n");
    return 0;
}