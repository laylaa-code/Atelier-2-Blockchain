#include "blockchain_base.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <tuple>

using std::cout;
using std::string;
using std::vector;

// Les evenements et les delais sont simules : aucun vrai reseau n'est utilise.
enum class Attaque { Majorite51, SelfishMining, BlockWithholding, Sybil };
enum class Etat { Reference, Attaquee, Protegee };
enum class Detecteur { Fixe, Adaptatif };

string nom(Attaque a) {
    switch (a) {
        case Attaque::Majorite51: return "51_pourcent";
        case Attaque::SelfishMining: return "selfish_mining";
        case Attaque::BlockWithholding: return "block_withholding";
        default: return "sybil";
    }
}
string nom(Etat e) {
    if (e == Etat::Reference) return "reference";
    return e == Etat::Attaquee ? "attaquee" : "protegee";
}

struct Scenario {
    Attaque attaque;
    int noeuds = 12;
    double puissance = 0.60; // part de hash du mineur attaqueur (PoW)
    int latenceMs = 60;
    int strategie = 1;        // agressivite / nombre d'identites selon l'attaque
    int tours = 80;
    unsigned graine = 7;
};

struct Mesures {
    int blocs = 0;              // blocs trouves, y compris les blocs rejetes
    int orphelins = 0;
    int incoherences = 0;       // annonces de chaines contradictoires
    int publicationsTardives = 0;
    int reorganisations = 0;
    int transactionsDoubleDepense = 0;
    int sharesSuspect = 0;
    int sharesAutres = 0;
    int blocsSuspectPool = 0;
    int blocsAutresPool = 0;
    int identitesMemeOrigine = 0;
    int voisinsTotal = 0;
    int voisinsSybil = 0;
    int toursIsole = 0;
    int recompensesAttaquant = 0;
    int recompensesHonnetes = 0;
    int messagesNormaux = 0;
    int messagesControle = 0;
    int premiereAlerte = -1;   // tour de la premiere detection
    double sommeDelais = 0;
    int nombreDelais = 0;
    double pertes = 0;         // unite de recompense, sauf double depense = 10
    bool succes = false;
    bool chaineValide = true;

    double tauxOrphelins() const {
        return blocs ? double(orphelins) / blocs : 0.0;
    }
    double delaiMoyen() const {
        return nombreDelais ? sommeDelais / nombreDelais : 0.0;
    }
    double partRecompenses() const {
        int total = recompensesAttaquant + recompensesHonnetes;
        return total ? double(recompensesAttaquant) / total : 0.0;
    }
    double surcout() const {
        return messagesNormaux ? 100.0 * messagesControle / messagesNormaux : 0.0;
    }
    bool alerte() const { return premiereAlerte >= 0; }
};

const vector<Validateur> VALIDATEURS = {
    {"Alice", 50}, {"Bob", 30}, {"Chloe", 20}
};

Bloc creerBloc(const vector<Bloc>& chaine, const string& tx, bool pos = false) {
    Bloc bloc(static_cast<int>(chaine.size()), {tx}, chaine.back().hash);
    if (pos) {
        bloc.validateur = "Alice"; // Alice a une mise positive dans l'atelier 1
        bloc.hash = bloc.calculerHash();
    } else {
        bloc.minerPoW(1); // difficulte 1, pour rendre les experiences rapides
    }
    return bloc;
}

bool verifierChaine(const vector<Bloc>& chaine, bool pos = false) {
    if (chaine.empty() || chaine[0].hashPrecedent != "0") return false;
    for (size_t i = 0; i < chaine.size(); ++i) {
        const Bloc& b = chaine[i];
        if (b.hash != b.calculerHash()) return false;
        if (i == 0) continue;
        if (b.index != static_cast<int>(i) || b.hashPrecedent != chaine[i-1].hash) return false;
        if (pos) {
            if (b.validateur != "Alice") return false;
        } else if (!commenceParZeros(b.hash, 1)) return false;
    }
    return true;
}

// Les seuils adaptatifs tiennent compte d'un essai normal avec les memes
// conditions de reseau. Le detecteur fixe sert de methode de comparaison.
bool suspect(const Mesures& m, const Mesures& normal, Detecteur d, int noeuds) {
    double limiteOrphelins = (d == Detecteur::Adaptatif)
        ? std::min(0.45, std::max(0.14, normal.tauxOrphelins() + 0.12)) : 0.25;
    double limiteDelai = (d == Detecteur::Adaptatif)
        ? normal.delaiMoyen() * 1.45 + 20.0 : 170.0;

    // Reorganisation, publication suspecte ou fourche visible.
    if (m.reorganisations > 0 && m.incoherences >= 2) return true;
    if (m.publicationsTardives >= 2 && m.orphelins >= 2 &&
        (m.tauxOrphelins() > limiteOrphelins || m.delaiMoyen() > limiteDelai)) return true;
    double limiteRecompenses = (d == Detecteur::Adaptatif)
        ? std::min(0.85, normal.partRecompenses() + 0.20) : 0.60;
    if (m.publicationsTardives >= 2 && m.orphelins >= 2 &&
        m.recompensesAttaquant + m.recompensesHonnetes >= 12 &&
        m.partRecompenses() > limiteRecompenses) return true;

    // Dans une pool, un mineur remet des shares mais pratiquement jamais un bloc.
    if (m.sharesSuspect >= 14 && m.blocsAutresPool >= 3 && m.sharesAutres >= 14) {
        double tauxSuspect = double(m.blocsSuspectPool) / m.sharesSuspect;
        double tauxAutres = double(m.blocsAutresPool) / m.sharesAutres;
        if (tauxSuspect < tauxAutres * 0.25) return true;
    }

    // Plusieurs identites reliees a une seule origine diffusent des versions differentes.
    if (m.identitesMemeOrigine > std::max(3, noeuds / 3) && m.incoherences >= 2)
        return true;
    return false;
}

// Mise a jour des mesures communes et detection : une alerte n'est jamais
// declenchee a partir d'une etiquette de verite connue du simulateur.
void observer(Mesures& m, const Scenario& s, const Mesures& normal,
              Detecteur det, int tour, bool protection) {
    m.messagesNormaux += 3 * s.noeuds;
    m.sommeDelais += s.latenceMs;
    ++m.nombreDelais;
    if (protection && tour % 5 == 0) m.messagesControle += s.noeuds;
    if (m.premiereAlerte < 0 && suspect(m, normal, det, s.noeuds)) {
        m.premiereAlerte = tour;
        if (protection) m.messagesControle += 2 * s.noeuds; // annonce d'alerte
    }
}

Mesures simulation51(const Scenario& s, Etat e, const Mesures& normal, Detecteur det) {
    const bool attaque = e != Etat::Reference;
    const bool protection = e == Etat::Protegee;
    std::mt19937 gen(s.graine);
    std::bernoulli_distribution mineurAdverse(s.puissance);
    std::uniform_real_distribution<double> hasard(0.0, 1.0);
    Mesures m;

    // Bloc de depart et premier paiement, crees comme dans l'atelier 1.
    Blockchain depart(Consensus::ProofOfWork, 1, VALIDATEURS);
    vector<Bloc> publique = depart.obtenirBlocs();
    vector<Bloc> privee = publique; // depart identique pour les deux branches
    publique.push_back(creerBloc(publique, "Alice -> Marchand : 10 MAD"));
    ++m.blocs;
    ++m.recompensesHonnetes;
    bool reorganisationFaite = false;
    const int confirmationsAvantLivraison = protection ? 6 : 1;
    bool marchandiseLivree = !protection; // premier bloc deja publie

    for (int t = 1; t <= s.tours; ++t) {
        bool adverse = mineurAdverse(gen);
        if (attaque && !reorganisationFaite && adverse) {
            string tx = privee.size() == 1
                ? "Alice -> Complice : 10 MAD" : "Bloc prive attaque " + std::to_string(t);
            privee.push_back(creerBloc(privee, tx));
            ++m.blocs;
            m.sommeDelais += s.latenceMs * 0.5; // bloc garde secret
        } else {
            publique.push_back(creerBloc(publique, "Transaction normale " + std::to_string(t)));
            ++m.blocs;
            ++m.recompensesHonnetes;
        }
        if (static_cast<int>(publique.size()) - 1 >= confirmationsAvantLivraison)
            marchandiseLivree = true;

        // La branche cachee est publiee lorsqu'elle devient plus longue.
        if (attaque && !reorganisationFaite &&
            static_cast<int>(privee.size()) - static_cast<int>(publique.size()) >= s.strategie) {
            int blocsHonnetesAnnules = static_cast<int>(publique.size()) - 1;
            m.orphelins += blocsHonnetesAnnules;
            m.recompensesHonnetes -= blocsHonnetesAnnules;
            m.recompensesAttaquant += static_cast<int>(privee.size()) - 1;
            m.incoherences += 3;
            ++m.reorganisations;
            ++m.transactionsDoubleDepense;
            if (marchandiseLivree) {
                m.pertes += 10.0;
                m.succes = true;
            }
            publique = privee;
            reorganisationFaite = true;
        }
        // Quelques fourches naturelles de faible ampleur sont possibles.
        if (hasard(gen) < s.latenceMs / 3000.0) { ++m.orphelins; ++m.blocs; }
        observer(m, s, normal, det, t, protection);
    }
    m.chaineValide = verifierChaine(publique);
    return m;
}

Mesures simulationSelfish(const Scenario& s, Etat e, const Mesures& normal, Detecteur det) {
    const bool attaque = e != Etat::Reference;
    const bool protection = e == Etat::Protegee;
    std::mt19937 gen(s.graine);
    std::bernoulli_distribution mineurAdverse(s.puissance);
    std::uniform_real_distribution<double> hasard(0.0, 1.0);
    Mesures m;
    Blockchain depart(Consensus::ProofOfWork, 1, VALIDATEURS);
    vector<Bloc> publique = depart.obtenirBlocs(), privee = publique;
    int avancePrivee = 0;

    for (int t = 1; t <= s.tours; ++t) {
        bool adverse = mineurAdverse(gen);
        if (attaque && adverse) {
            if (avancePrivee == 0) privee = publique;
            privee.push_back(creerBloc(privee, "Bloc prive " + std::to_string(t)));
            ++avancePrivee;
            ++m.blocs;
        } else if (attaque && avancePrivee > 0) {
            // Un mineur honnete trouve un bloc avant publication du bloc cache.
            publique.push_back(creerBloc(publique, "Bloc honnete " + std::to_string(t)));
            ++m.blocs;
            if (avancePrivee >= 2) {
                // Publication de deux blocs ou plus : le bloc honnete devient orphelin.
                ++m.orphelins;
                m.pertes += 1;
                m.recompensesAttaquant += avancePrivee;
                m.publicationsTardives += avancePrivee;
                m.incoherences++;
                ++m.reorganisations;
                publique = privee;
            } else {
                // Egalite des branches : le meilleur relais reseau gagne plus souvent.
                double chanceAttaquant = (protection && m.alerte()) ? 0.12 :
                    std::min(0.75, 0.30 + s.latenceMs / 500.0 + (s.strategie - 1) * 0.08);
                if (hasard(gen) < chanceAttaquant) {
                    ++m.orphelins; ++m.recompensesAttaquant;
                    ++m.publicationsTardives; ++m.incoherences;
                    ++m.reorganisations; ++m.pertes;
                    publique = privee;
                } else {
                    ++m.orphelins; ++m.recompensesHonnetes;
                }
            }
            avancePrivee = 0;
            privee = publique;
        } else {
            publique.push_back(creerBloc(publique, "Bloc normal " + std::to_string(t)));
            ++m.blocs;
            if (adverse) ++m.recompensesAttaquant;
            else ++m.recompensesHonnetes;
            privee = publique;
        }
        // Les blocs publies en retard sont plus lents a atteindre le reseau.
        if (attaque && m.publicationsTardives > 0)
            m.sommeDelais += s.latenceMs * 0.65;
        if (hasard(gen) < s.latenceMs / 3000.0) { ++m.orphelins; ++m.blocs; }
        // Un reseau honnete peut aussi subir de vrais retards de publication.
        if (!attaque && hasard(gen) < s.latenceMs / 2500.0) {
            ++m.publicationsTardives;
            m.sommeDelais += 4.0 * s.latenceMs;
        }
        observer(m, s, normal, det, t, protection);
    }
    // Les blocs prives jamais publies sont perdus pour leur mineur.
    m.orphelins += avancePrivee;
    m.chaineValide = verifierChaine(publique);
    // Reussite : au moins une publication tardive adoptee par le reseau.
    // Cela ne signifie pas que la strategie est rentable a long terme.
    m.succes = attaque && m.reorganisations > 0 && m.recompensesAttaquant > 0;
    return m;
}

Mesures simulationPool(const Scenario& s, Etat e, const Mesures& normal, Detecteur det) {
    const bool attaque = e != Etat::Reference;
    const bool protection = e == Etat::Protegee;
    std::mt19937 gen(s.graine);
    std::uniform_real_distribution<double> hasard(0.0, 1.0);
    Mesures m;
    Blockchain depart(Consensus::ProofOfWork, 1, VALIDATEURS);
    vector<Bloc> chaine = depart.obtenirBlocs();
    bool exclu = false;
    double part = s.puissance; // proportion d'essais du mineur suspect dans la pool

    for (int t = 1; t <= s.tours; ++t) {
        // Des shares representent le travail remis a la pool (maquette simplifiee).
        bool suspectTravaille = !exclu && hasard(gen) < part;
        if (suspectTravaille) {
            ++m.sharesSuspect;
            // Le mineur garde parfois pour lui les blocs complets qu'il a trouves.
            if (hasard(gen) < 0.30) {
                bool cache = attaque && hasard(gen) < (s.strategie == 1 ? 0.80 : 1.0);
                if (cache) {
                    ++m.blocs;
                    ++m.pertes; // recompense de bloc perdue par la pool
                } else {
                    chaine.push_back(creerBloc(chaine, "Bloc pool mineur suspect " + std::to_string(t)));
                    ++m.blocsSuspectPool;
                    ++m.recompensesAttaquant;
                    ++m.blocs;
                }
            }
        } else if (!exclu || hasard(gen) < (1.0 - part)) {
            ++m.sharesAutres;
            if (hasard(gen) < 0.30) {
                chaine.push_back(creerBloc(chaine, "Bloc pool honnete " + std::to_string(t)));
                ++m.blocsAutresPool;
                ++m.recompensesHonnetes;
                ++m.blocs;
            }
        }
        if (hasard(gen) < s.latenceMs / 5000.0) { ++m.orphelins; ++m.blocs; }
        observer(m, s, normal, det, t, protection);
        if (protection && m.alerte() && !exclu) {
            exclu = true;
            m.messagesControle += s.noeuds; // exclusion annoncee aux membres
        }
    }
    m.chaineValide = verifierChaine(chaine);
    m.succes = m.pertes > 0;
    return m;
}

Mesures simulationSybil(const Scenario& s, Etat e, const Mesures& normal, Detecteur det) {
    const bool attaque = e != Etat::Reference;
    const bool protection = e == Etat::Protegee;
    std::mt19937 gen(s.graine);
    std::uniform_real_distribution<double> hasard(0.0, 1.0);
    Mesures m;
    Blockchain chaine(Consensus::ProofOfStake, 1, VALIDATEURS);
    const int identitesFactices = attaque ? s.noeuds * (s.strategie + 1) : 0;
    const int contacts = std::min(6, std::max(3, s.noeuds - 1));
    int consecutifsIsole = 0;

    for (int t = 1; t <= s.tours; ++t) {
        // Tous les faux comptes ont la meme origine et aucune mise PoS.
        int fauxVoisins = 0;
        for (int i = 0; i < contacts; ++i) {
            if (attaque && hasard(gen) < double(identitesFactices) /
                                      (identitesFactices + s.noeuds)) ++fauxVoisins;
        }
        m.identitesMemeOrigine = identitesFactices;
        m.voisinsTotal += contacts;
        m.voisinsSybil += fauxVoisins;
        if (attaque && fauxVoisins > 0) {
            ++m.incoherences; // faux comptes annoncent un autre sommet de chaine
            m.sommeDelais += s.latenceMs * double(fauxVoisins) / contacts;
        }
        // Apres l'alerte, un seul contact par origine est conserve.
        if (protection && m.alerte()) {
            fauxVoisins = std::min(1, fauxVoisins);
            m.messagesControle += contacts; // controle des identites et des pairs
        }
        if (fauxVoisins >= (2 * contacts + 2) / 3) {
            ++m.toursIsole;
            ++consecutifsIsole;
            m.pertes += 1; // indisponibilite du noeud cible : cout fictif
        } else {
            consecutifsIsole = 0;
        }
        if (consecutifsIsole >= 3) m.succes = true;
        if (t % 4 == 0) {
            chaine.ajouterBloc({"Transaction PoS " + std::to_string(t)});
            ++m.blocs;
            ++m.recompensesHonnetes; // seules les mises reconnues valident
        }
        observer(m, s, normal, det, t, protection);
    }
    m.chaineValide = chaine.estValide();
    return m;
}

Mesures executer(const Scenario& s, Etat e, const Mesures& normal, Detecteur d) {
    switch (s.attaque) {
        case Attaque::Majorite51: return simulation51(s, e, normal, d);
        case Attaque::SelfishMining: return simulationSelfish(s, e, normal, d);
        case Attaque::BlockWithholding: return simulationPool(s, e, normal, d);
        default: return simulationSybil(s, e, normal, d);
    }
}

struct Ligne { Scenario s; Etat etat; Mesures m; Detecteur det; };
string nom(Detecteur d) { return d == Detecteur::Fixe ? "fixe" : "adaptatif"; }

void ecrireEntete(std::ofstream& f) {
    f << "attaque,etat,detecteur,noeuds,puissance,latence_ms,strategie,graine,tours,"
         "succes,reorganisations,double_depense,taux_orphelins,delai_moyen_ms,"
         "part_recompenses_attaquant,incoherences,alerte,temps_detection_tours,"
         "pertes_unites,surcout_controle_pourcent,chaine_valide\n";
}
void ecrireLigne(std::ofstream& f, const Ligne& x) {
    const Mesures& m = x.m;
    f << nom(x.s.attaque) << ',' << nom(x.etat) << ',' << nom(x.det) << ','
      << x.s.noeuds << ',' << x.s.puissance << ',' << x.s.latenceMs << ','
      << x.s.strategie << ',' << x.s.graine << ',' << x.s.tours << ','
      << (m.succes ? 1 : 0) << ',' << m.reorganisations << ','
      << m.transactionsDoubleDepense << ',' << m.tauxOrphelins() << ','
      << m.delaiMoyen() << ',' << m.partRecompenses() << ','
      << m.incoherences << ',' << (m.alerte() ? 1 : 0) << ','
      << m.premiereAlerte << ',' << m.pertes << ',' << m.surcout() << ','
      << (m.chaineValide ? 1 : 0) << '\n';
}

struct Regroupement {
    int n = 0, succes = 0, alertes = 0;
    double pertes = 0, orphelins = 0, delais = 0, cout = 0, temps = 0;
    int detectes = 0;
    void ajouter(const Mesures& m) {
        ++n; succes += m.succes; alertes += m.alerte();
        pertes += m.pertes; orphelins += m.tauxOrphelins();
        delais += m.delaiMoyen(); cout += m.surcout();
        if (m.alerte()) { temps += m.premiereAlerte; ++detectes; }
    }
};

void experiences() {
    std::filesystem::create_directories("resultats");
    std::ofstream detail("resultats/experiences.csv");
    std::ofstream resume("resultats/resume.csv");
    std::ofstream detection("resultats/comparaison_detection.csv");
    if (!detail || !resume || !detection) throw std::runtime_error("Impossible d'ouvrir les fichiers CSV.");
    detail << std::fixed << std::setprecision(4);
    resume << std::fixed << std::setprecision(4);
    detection << std::fixed << std::setprecision(4);
    ecrireEntete(detail);

    using Cle = std::tuple<string,string,string>;
    std::map<Cle, Regroupement> groupes;
    struct Confusion {int vp=0, fp=0, fn=0, vn=0; double t=0; int n=0;};
    std::map<std::pair<string,string>, Confusion> matrices;
    int simulations = 0;
    for (Attaque a : {Attaque::Majorite51, Attaque::SelfishMining,
                      Attaque::BlockWithholding, Attaque::Sybil}) {
        for (int noeuds : {8, 16})
        for (int latence : {40, 150})
        for (int strategie : {1, 2})
        for (int niveau = 0; niveau < 2; ++niveau)
        for (unsigned graine : {7u, 19u, 31u}) {
            Scenario s{a, noeuds, a == Attaque::Majorite51 ?
                (niveau == 0 ? 0.55 : 0.70) :
                (niveau == 0 ? 0.25 : 0.40), latence, strategie, 80, graine};
            // Calibrage sur un essai sans attaque, avec une autre graine.
            Scenario apprentissage = s;
            apprentissage.graine = 2027u;
            Mesures normal = executer(apprentissage, Etat::Reference, Mesures{}, Detecteur::Fixe);
            for (Detecteur d : {Detecteur::Fixe, Detecteur::Adaptatif}) {
                for (Etat etat : {Etat::Reference, Etat::Attaquee, Etat::Protegee}) {
                    Mesures m = executer(s, etat, normal, d);
                    if (!m.chaineValide) throw std::runtime_error("Chaine invalide dans la simulation !");
                    Ligne ligne{s, etat, m, d};
                    ecrireLigne(detail, ligne);
                    groupes[{nom(a),nom(etat),nom(d)}].ajouter(m);
                    Confusion& c = matrices[{nom(a),nom(d)}];
                    bool attaquePresente = etat != Etat::Reference;
                    if (attaquePresente && m.alerte()) { ++c.vp; c.t += m.premiereAlerte; ++c.n; }
                    else if (attaquePresente) ++c.fn;
                    else if (m.alerte()) ++c.fp;
                    else ++c.vn;
                    ++simulations;
                }
            }
        }
    }
    resume << "attaque,etat,detecteur,essais,taux_succes_pourcent,"
           << "perte_moyenne_unites,orphelins_pourcent,delai_moyen_ms,"
           << "alertes_pourcent,temps_detection_moyen_tours,surcout_controle_pourcent\n";
    for (const auto& kv : groupes) {
        const auto& [a,e,d] = kv.first;
        const auto& x = kv.second;
        resume << a << ',' << e << ',' << d << ',' << x.n << ','
               << 100.0*x.succes/x.n << ',' << x.pertes/x.n << ','
               << 100.0*x.orphelins/x.n << ',' << x.delais/x.n << ','
               << 100.0*x.alertes/x.n << ','
               << (x.detectes ? x.temps/x.detectes : -1) << ',' << x.cout/x.n << '\n';
    }
    detection << "attaque,detecteur,VP,FP,FN,VN,precision_pourcent,rappel_pourcent,"
              << "temps_detection_moyen_tours\n";
    for (const auto& kv : matrices) {
        const auto& [a,d] = kv.first;
        const auto& c = kv.second;
        double precision = c.vp+c.fp ? 100.0*c.vp/(c.vp+c.fp) : 0.0;
        double rappel = c.vp+c.fn ? 100.0*c.vp/(c.vp+c.fn) : 0.0;
        detection << a << ',' << d << ',' << c.vp << ',' << c.fp << ','
                  << c.fn << ',' << c.vn << ',' << precision << ',' << rappel
                  << ',' << (c.n ? c.t/c.n : -1.0) << '\n';
    }
    cout << "Experiences terminees : " << simulations << " simulations.\n"
         << "Fichiers : resultats/experiences.csv, resultats/resume.csv, "
         << "resultats/comparaison_detection.csv\n";
}

void demonstration() {
    cout << "=== ATELIER 2 - SIMULATION DES ATTAQUES ===\n";
    cout << "Chaque scenario est simule avec et sans attaque, puis avec protection.\n"
         << "Graine fixe = 7. Les delais representent un reseau simule.\n\n";
    cout << std::fixed << std::setprecision(2);
    for (Attaque a : {Attaque::Majorite51, Attaque::SelfishMining,
                      Attaque::BlockWithholding, Attaque::Sybil}) {
        Scenario s{a, 12, a == Attaque::Majorite51 ? 0.65 : 0.35, 100, 2, 80, 7};
        Scenario apprentissage = s; apprentissage.graine = 2027;
        Mesures normal = executer(apprentissage, Etat::Reference, Mesures{}, Detecteur::Fixe);
        cout << "--- " << nom(a) << " ---\n";
        for (Etat e : {Etat::Reference, Etat::Attaquee, Etat::Protegee}) {
            Mesures m = executer(s, e, normal, Detecteur::Adaptatif);
            cout << "  " << nom(e) << " : succes=" << (m.succes ? "oui" : "non")
                 << ", pertes=" << m.pertes
                 << ", orphelins=" << m.tauxOrphelins()*100 << "%"
                 << ", alerte=" << (m.alerte() ? "oui" : "non")
                 << ", temps=" << m.premiereAlerte << " tours"
                 << ", cout controle=" << m.surcout() << "%"
                 << ", chaine " << (m.chaineValide ? "valide" : "INVALIDE") << '\n';
        }
        cout << '\n';
    }
}

void tests() {
    Blockchain pow(Consensus::ProofOfWork, 1, VALIDATEURS);
    Blockchain pos(Consensus::ProofOfStake, 1, VALIDATEURS);
    pow.ajouterBloc({"Alice -> Bob : 10 MAD"});
    pos.ajouterBloc({"Alice -> Bob : 10 MAD"});
    if (!pow.estValide() || !pos.estValide()) throw std::runtime_error("PoW ou PoS invalide");
    pow.modifierTransactionPourTest(1, 0, "Alice -> Bob : 1000 MAD");
    if (pow.estValide()) throw std::runtime_error("Modification non detectee");
    for (Attaque a : {Attaque::Majorite51, Attaque::SelfishMining,
                      Attaque::BlockWithholding, Attaque::Sybil}) {
        Scenario s{a, 12, a == Attaque::Majorite51 ? 0.70 : 0.40, 100, 2, 80, 7};
        Mesures m = executer(s, Etat::Attaquee, Mesures{}, Detecteur::Adaptatif);
        if (!m.chaineValide) throw std::runtime_error("Simulation invalide : " + nom(a));
    }
    cout << "Tests OK : PoW, PoS, integrite, et quatre scenarios.\n";
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            cout << "Utilisation : atelier2 --demo | --test | --experiences\n";
            return 1;
        }
        string option = argv[1];
        if (option == "--demo") demonstration();
        else if (option == "--test") tests();
        else if (option == "--experiences") experiences();
        else { cout << "Option inconnue.\n"; return 1; }
    } catch (const std::exception& e) {
        std::cerr << "Erreur : " << e.what() << '\n';
        return 1;
    }
    return 0;
}
