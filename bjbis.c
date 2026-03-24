#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_JOUEURS 3
#define MAX_CARTES 12
#define BANKROLL_INITIAL 1000


typedef enum { COEUR, CARREAU, TREFLE, PIQUE } Couleur;
typedef enum { 
    DEUX=2, TROIS, QUATRE, CINQ, SIX, SEPT, HUIT, NEUF, DIX,
    VALET=11, DAME=12, ROI=13, AS=14
} Rang;

typedef enum {
    ETAT_SELECTION_JOUEURS,
    ETAT_PARI,
    ETAT_TOUR_JOUEUR,
    ETAT_TOUR_CROUPIER,
    ETAT_ROUND_TERMINE,
    ETAT_JEU_TERMINE
} EtatJeu;


typedef struct {
    Couleur couleur;
    Rang rang;
} Carte;

typedef struct {
    Carte* cartes[MAX_CARTES];
    int nb_cartes;
    int valeur;
} Main;

typedef struct {
    char nom[50];
    Main* main;
    int bankroll;
    int pari;
    int actif;
    int a_accepte;
    int est_busted;
} Joueur;

typedef struct {
    Joueur joueurs[MAX_JOUEURS];
    int nb_joueurs;
    int index_joueur_actuel;
} GestionnaireJoueurs;

typedef struct {
    Carte cartes[364];  
    int index_prochaine_carte;
    int nb_cartes;
} Sabot;

typedef struct {
    EtatJeu etat;
    GestionnaireJoueurs* gestionnaire_joueurs;
    Main* main_croupier;
    Sabot* sabot;
    int numero_round;
} Jeu;




int valeur_carte(Carte* carte) {
    
    if (carte->rang == AS) return 11;
    
    if (carte->rang >= 10) return 10;
    
    return carte->rang;
}


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


const char* couleur_vers_string(Couleur couleur) {
    
    switch(couleur) {
        case COEUR: return "â™¥";
        case CARREAU: return "â™¦";
        case TREFLE: return "â™£";
        case PIQUE: return "â™ ";
        default: return "?";
    }
}


void afficher_carte(Carte* carte) {
    
    printf("[%s%s]", rang_vers_string(carte->rang), couleur_vers_string(carte->couleur));
}




Main* creer_main() {
    
    Main* main = malloc(sizeof(Main));
    main->nb_cartes = 0;
    main->valeur = 0;
    
    for (int i = 0; i < MAX_CARTES; i++) { 
        main->cartes[i] = NULL;
    }
    return main;
}


void detruire_main(Main* main) {
    
    if (main) free(main);
}


int calculer_valeur_main(Main* main) {
    
    if (!main || main->nb_cartes == 0) return 0;
    
    int valeur = 0;
    int as = 0; 
    
    
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
    
    
    while (valeur > 21 && as > 0) {
        valeur -= 10; 
        as--;
    }
    
    main->valeur = valeur;
    return valeur;
}


void ajouter_carte_main(Main* main, Carte* carte) {
    
    if (main->nb_cartes < MAX_CARTES) {
        main->cartes[main->nb_cartes++] = carte;
        
        calculer_valeur_main(main);
    }
}


int est_blackjack(Main* main) {
    
    return (main->nb_cartes == 2 && main->valeur == 21);
}


int est_busted(Main* main) {
    
    return (main->valeur > 21);
}


int peut_scinder_main(Main* main) {
    
    return (main->nb_cartes == 2 && 
            main->cartes[0]->rang == main->cartes[1]->rang);
}


void afficher_main(Main* main, int cacher_deuxieme) {
    
    if (main->nb_cartes == 0) return;
    
    
    for (int i = 0; i < main->nb_cartes; i++) {
        
        if (cacher_deuxieme && i == 1) {
            printf("[??]");
        } else {
            afficher_carte(main->cartes[i]);
        }
        printf(" ");
    }
    
    
    if (!cacher_deuxieme) {
        printf("(Valeur: %d)", main->valeur);
    }
}

void reinitialiser_main(Main* main) {
    
    main->nb_cartes = 0;
    main->valeur = 0;
    
    for (int i = 0; i < MAX_CARTES; i++) {
        main->cartes[i] = NULL;
    }
}




Sabot* creer_sabot() {
    
    Sabot* sabot = malloc(sizeof(Sabot));
    sabot->index_prochaine_carte = 0;
    sabot->nb_cartes = 0;
    
    
    for (int d = 0; d < 7; d++) {
        
        for (Couleur c = COEUR; c <= PIQUE; c++) {
            
            for (Rang r = DEUX; r <= AS; r++) {
                
                sabot->cartes[sabot->nb_cartes].couleur = c;
                sabot->cartes[sabot->nb_cartes].rang = r;
                sabot->nb_cartes++;
            }
        }
    }
    
    return sabot;
}


void detruire_sabot(Sabot* sabot) {
    
    if (sabot) free(sabot);
}


void melanger_sabot(Sabot* sabot) {
    
    srand(time(NULL));
    
    
    for (int i = 0; i < sabot->nb_cartes; i++) {
        
        int j = rand() % sabot->nb_cartes;
        
        Carte temp = sabot->cartes[i];
        sabot->cartes[i] = sabot->cartes[j];
        sabot->cartes[j] = temp;
    }
    
    sabot->index_prochaine_carte = 0;
}


Carte* tirer_carte(Sabot* sabot) {
    
    if (sabot->index_prochaine_carte >= sabot->nb_cartes) return NULL;
    
    return &sabot->cartes[sabot->index_prochaine_carte++];
}




Joueur creer_joueur(const char* nom) {
    
    Joueur j;
    
    strcpy(j.nom, nom);
    
    j.main = creer_main();
    
    j.bankroll = BANKROLL_INITIAL;
    j.pari = 0;
    j.actif = 1;
    j.a_accepte = 0;
    j.est_busted = 0;
    return j;
}


void detruire_joueur(Joueur* j) {
    
    if (j && j->main) detruire_main(j->main);
}


void placer_pari(Joueur* j, int montant) {
    
    if (montant > 0 && montant <= j->bankroll) {
        j->pari = montant;
        
        j->bankroll -= montant;
        printf("%s mise: $%d\n", j->nom, montant);
    }
}


void reinitialiser_joueur_pour_round(Joueur* j) {
    
    reinitialiser_main(j->main);
    
    j->pari = 0;
    j->actif = 1;
    j->a_accepte = 0;
    j->est_busted = 0;
}




GestionnaireJoueurs* creer_gestionnaire_joueurs(int nb) {
    
    GestionnaireJoueurs* gj = malloc(sizeof(GestionnaireJoueurs));
    gj->nb_joueurs = nb;
    gj->index_joueur_actuel = 0;
    
    
    for (int i = 0; i < nb; i++) {
        char nom[20];
        
        snprintf(nom, sizeof(nom), "Joueur %d", i + 1);
        
        gj->joueurs[i] = creer_joueur(nom);
    }
    
    return gj;
}


void detruire_gestionnaire_joueurs(GestionnaireJoueurs* gj) {
    
    if (gj) {
        
        for (int i = 0; i < gj->nb_joueurs; i++) {
            detruire_joueur(&gj->joueurs[i]);
        }
        
        free(gj);
    }
}


Joueur* obtenir_joueur_actuel(GestionnaireJoueurs* gj) {
    
    if (gj && gj->index_joueur_actuel < gj->nb_joueurs) {
        return &gj->joueurs[gj->index_joueur_actuel];
    }
    return NULL;
}


int joueur_suivant(GestionnaireJoueurs* gj) {
    
    gj->index_joueur_actuel++;
    
    return gj->index_joueur_actuel < gj->nb_joueurs;
}


int tous_joueurs_ruines(GestionnaireJoueurs* gj) {
    
    for (int i = 0; i < gj->nb_joueurs; i++) {
        
        if (gj->joueurs[i].bankroll > 0) return 0;
    }
    
    return 1;
}




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


void detruire_jeu(Jeu* j) {
    
    if (j) {
        
        if (j->main_croupier) detruire_main(j->main_croupier);
        
        if (j->sabot) detruire_sabot(j->sabot);
        
        if (j->gestionnaire_joueurs) detruire_gestionnaire_joueurs(j->gestionnaire_joueurs);
        
        free(j);
    }
}


void demarrer_jeu_avec_joueurs(Jeu* j, int nb) {
    
    if (j->gestionnaire_joueurs) detruire_gestionnaire_joueurs(j->gestionnaire_joueurs);
    
    j->gestionnaire_joueurs = creer_gestionnaire_joueurs(nb);
    
    j->etat = ETAT_PARI;
    j->numero_round = 1;
}


void distribuer_cartes_initiales(Jeu* j) {
    
    for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
        Joueur* p = &j->gestionnaire_joueurs->joueurs[i];
        
        if (p->actif && p->pari > 0) {
            
            ajouter_carte_main(p->main, tirer_carte(j->sabot));
            ajouter_carte_main(p->main, tirer_carte(j->sabot));
        }
    }
    
    
    ajouter_carte_main(j->main_croupier, tirer_carte(j->sabot));
    ajouter_carte_main(j->main_croupier, tirer_carte(j->sabot));
    
    
    j->etat = ETAT_TOUR_JOUEUR;
    printf("\nðŸ“‹ Cartes distribuÃ©es!\n\n");
}


void joueur_tire(Jeu* j) {
    
    Joueur* j_act = obtenir_joueur_actuel(j->gestionnaire_joueurs);
    
    if (j_act) {
        
        ajouter_carte_main(j_act->main, tirer_carte(j->sabot));
        printf("%s tire. Nouvelle valeur: %d\n", j_act->nom, j_act->main->valeur);
        
        
        if (est_busted(j_act->main)) {
            j_act->est_busted = 1;
            printf("ðŸ’¥ %s BUST!\n\n", j_act->nom);
            
            joueur_suivant(j->gestionnaire_joueurs);
        }
    }
}


void joueur_accepte(Jeu* j) {
    
    Joueur* j_act = obtenir_joueur_actuel(j->gestionnaire_joueurs);
    
    if (j_act) {
        
        j_act->a_accepte = 1;
        printf("%s accepte avec %d\n\n", j_act->nom, j_act->main->valeur);
        
        joueur_suivant(j->gestionnaire_joueurs);
    }
}


void jeu_croupier(Jeu* j) {
    printf("\nðŸŽ¯ Tour du croupier...\n");
    printf("Le croupier rÃ©vÃ¨le: ");
    afficher_main(j->main_croupier, 0);
    printf("\n\n");
    
    
    while (j->main_croupier->valeur < 17) {
        ajouter_carte_main(j->main_croupier, tirer_carte(j->sabot));
        printf("Le croupier tire: %d\n", j->main_croupier->valeur);
    }
    
    
    if (est_busted(j->main_croupier)) {
        printf("ðŸ’¥ Croupier BUST! (%d)\n\n", j->main_croupier->valeur);
    } else {
        printf("Le croupier accepte Ã  %d\n\n", j->main_croupier->valeur);
    }
    
    
    j->etat = ETAT_ROUND_TERMINE;
}


void evaluer_round(Jeu* j) {
    printf("â•â•â• RESULTATS ROUND %d â•â•â•\n", j->numero_round);
    printf("Croupier: %d points%s\n\n", 
           j->main_croupier->valeur,
           est_busted(j->main_croupier) ? " (BUST)" : "");
    
    int val_croupier = j->main_croupier->valeur;
    int croupier_bust = est_busted(j->main_croupier);
    
    
    for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
        Joueur* p = &j->gestionnaire_joueurs->joueurs[i];
        
        
        if (p->pari == 0) continue;
        
        int val_joueur = p->main->valeur;
        int joueur_bust = p->est_busted;
        int joueur_bj = est_blackjack(p->main);
        int croupier_bj = est_blackjack(j->main_croupier);
        
        printf("%s: %d", p->nom, val_joueur);
        if (joueur_bj) printf(" (BLACKJACK!)");
        if (joueur_bust) printf(" (BUST)");
        printf(" | Pari: $%d | ", p->pari);
        
        
        if (joueur_bj && !croupier_bj) {
            
            int gains = (p->pari * 3) / 2;
            p->bankroll += p->pari + gains;
            printf("GAGNE 3:2 +$%d\n", gains);
        } else if (joueur_bust) {
            
            printf("PERD -$%d\n", p->pari);
        } else if (croupier_bust) {
            
            p->bankroll += p->pari * 2;
            printf("GAGNE +$%d\n", p->pari);
        } else if (croupier_bj && !joueur_bj) {
            
            printf("PERD -$%d\n", p->pari);
        } else if (val_joueur > val_croupier) {
            
            p->bankroll += p->pari * 2;
            printf("GAGNE +$%d\n", p->pari);
        } else if (val_joueur < val_croupier) {
            
            printf("PERD -$%d\n", p->pari);
        } else {
            
            p->bankroll += p->pari;
            printf("EGALITE =$0\n");
        }
        
        printf("Bankroll: $%d\n\n", p->bankroll);
    }
}


void demarrer_nouveau_round(Jeu* j) {
    
    j->numero_round++;
    
    
    for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
        reinitialiser_joueur_pour_round(&j->gestionnaire_joueurs->joueurs[i]);
    }
    
    
    reinitialiser_main(j->main_croupier);
    j->gestionnaire_joueurs->index_joueur_actuel = 0;
    
    
    if (j->sabot->index_prochaine_carte > j->sabot->nb_cartes - 20) {
        melanger_sabot(j->sabot);
        printf("ðŸ”€ Sabot remÃ©langÃ©\n\n");
    }
    
    
    j->etat = ETAT_PARI;
}


void afficher_jeu(Jeu* j) {
    printf("\n");
    printf("â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n");
    
    
    if (j->etat == ETAT_TOUR_JOUEUR || 
        j->etat == ETAT_TOUR_CROUPIER ||
        j->etat == ETAT_ROUND_TERMINE) {
        
        printf("â•‘ CROUPIER: ");
        
        afficher_main(j->main_croupier, 
                    (j->etat == ETAT_TOUR_JOUEUR) ? 1 : 0);
        printf("\n");
        printf("â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£\n");
        
        
        for (int i = 0; i < j->gestionnaire_joueurs->nb_joueurs; i++) {
            Joueur* p = &j->gestionnaire_joueurs->joueurs[i];
            printf("â•‘ %s: ", p->nom);
            afficher_main(p->main, 0);
            printf("\n");
        }
    }
    
    printf("â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n");
}

int main() {
    printf("ðŸŽ° CASINO BLACKJACK\n\n");
    
    
    Jeu* jeu = creer_jeu();
    int quitter = 0;
    
    
    while (!quitter) {
        
        if (jeu->etat == ETAT_SELECTION_JOUEURS) {
            printf("Combien de joueurs? (1-3): ");
            int nb;
            scanf("%d", &nb);
            
            if (nb >= 1 && nb <= 3) {
                demarrer_jeu_avec_joueurs(jeu, nb);
            }
        }
        
        
        else if (jeu->etat == ETAT_PARI) {
            printf("\nâ•”â•â•â• PHASE DE PARI â•â•â•â•—\n");
            int tous_ont_parie = 1;
            
            
            for (int i = 0; i < jeu->gestionnaire_joueurs->nb_joueurs; i++) {
                Joueur* p = &jeu->gestionnaire_joueurs->joueurs[i];
                printf("\n%s - Bankroll: $%d\n", p->nom, p->bankroll);
                
                int pari = 0;
                printf("Entrez votre pari: $");
                scanf("%d", &pari);
                
                
                if (pari > 0 && pari <= p->bankroll) {
                    placer_pari(p, pari);
                    p->actif = 1;
                } else {
                    printf("Pari invalide!\n");
                    i--;
                    continue;
                }
            }
            
            
            distribuer_cartes_initiales(jeu);
        }
        
        
        else if (jeu->etat == ETAT_TOUR_JOUEUR) {
            afficher_jeu(jeu);
            
            Joueur* p = obtenir_joueur_actuel(jeu->gestionnaire_joueurs);
            
            if (!p || p->a_accepte || p->est_busted) {
                
                if (!joueur_suivant(jeu->gestionnaire_joueurs)) {
                    jeu->etat = ETAT_TOUR_CROUPIER;
                }
            } else {
                printf("\nTour de %s (Valeur: %d)\n", p->nom, p->main->valeur);
                printf("[1] Tirer  [2] Accepter: ");
                
                int choix;
                scanf("%d", &choix);
                
                
                if (choix == 1) {
                    joueur_tire(jeu);
                } else {
                    joueur_accepte(jeu);
                }
            }
        }
        
        
        else if (jeu->etat == ETAT_TOUR_CROUPIER) {
            afficher_jeu(jeu);
            jeu_croupier(jeu);
        }
        
        
        else if (jeu->etat == ETAT_ROUND_TERMINE) {
            evaluer_round(jeu);
            
            
            if (tous_joueurs_ruines(jeu->gestionnaire_joueurs)) {
                jeu->etat = ETAT_JEU_TERMINE;
            } else {
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
        
        
        else if (jeu->etat == ETAT_JEU_TERMINE) {
            printf("\nðŸŽ­ JEU TERMINE - SANS ARGENT!\n");
            printf("Classement final:\n");
            
            for (int i = 0; i < jeu->gestionnaire_joueurs->nb_joueurs; i++) {
                Joueur* p = &jeu->gestionnaire_joueurs->joueurs[i];
                printf("%s: $%d\n", p->nom, p->bankroll);
            }
            quitter = 1;
        }
    }
    
    
    detruire_jeu(jeu);
    printf("\nðŸ‘‹ Merci d'avoir jouÃ©!\n");
    return 0;
}
