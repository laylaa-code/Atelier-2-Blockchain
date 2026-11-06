#pragma once
// Base reprise de l exercice 4 de l atelier 1 (sans sa fonction main).
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>

using namespace std;

namespace sha256 {
    using std::array;
    using std::uint32_t;
    using std::uint64_t;
    using std::uint8_t;
    using std::string;
    using std::vector;

    static constexpr array<uint32_t, 64> K = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    inline uint32_t rotationDroite(uint32_t valeur, unsigned int decalage) {
        return (valeur >> decalage) | (valeur << (32 - decalage));
    }

    inline string hash(const string& texte) {
        vector<uint8_t> message;
        message.reserve(texte.size() + 72);
        for (unsigned char caractere : texte) {
            message.push_back(static_cast<uint8_t>(caractere));
        }

        const uint64_t longueurBits = static_cast<uint64_t>(message.size()) * 8ULL;
        message.push_back(0x80);
        while (message.size() % 64 != 56) {
            message.push_back(0x00);
        }
        for (int i = 7; i >= 0; --i) {
            message.push_back(static_cast<uint8_t>((longueurBits >> (i * 8)) & 0xff));
        }

        array<uint32_t, 8> h = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        for (size_t debut = 0; debut < message.size(); debut += 64) {
            array<uint32_t, 64> w{};
            for (size_t i = 0; i < 16; ++i) {
                const size_t p = debut + i * 4;
                w[i] = (static_cast<uint32_t>(message[p]) << 24)
                     | (static_cast<uint32_t>(message[p + 1]) << 16)
                     | (static_cast<uint32_t>(message[p + 2]) << 8)
                     | static_cast<uint32_t>(message[p + 3]);
            }

            for (size_t i = 16; i < 64; ++i) {
                const uint32_t s0 = rotationDroite(w[i - 15], 7)
                                  ^ rotationDroite(w[i - 15], 18)
                                  ^ (w[i - 15] >> 3);
                const uint32_t s1 = rotationDroite(w[i - 2], 17)
                                  ^ rotationDroite(w[i - 2], 19)
                                  ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }

            uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
            uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

            for (size_t i = 0; i < 64; ++i) {
                const uint32_t sigma1 = rotationDroite(e, 6)
                                      ^ rotationDroite(e, 11)
                                      ^ rotationDroite(e, 25);
                const uint32_t choix = (e & f) ^ ((~e) & g);
                const uint32_t temp1 = hh + sigma1 + choix + K[i] + w[i];
                const uint32_t sigma0 = rotationDroite(a, 2)
                                      ^ rotationDroite(a, 13)
                                      ^ rotationDroite(a, 22);
                const uint32_t majorite = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t temp2 = sigma0 + majorite;

                hh = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h[0] += a; h[1] += b; h[2] += c; h[3] += d;
            h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }

        std::ostringstream resultat;
        resultat << std::hex << std::setfill('0');
        for (uint32_t valeur : h) {
            resultat << std::setw(8) << valeur;
        }
        return resultat.str();
    }
}

long long tempsActuelEnMillisecondes() {
    return chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()).count();
}

enum class Consensus { ProofOfWork, ProofOfStake };

string nomConsensus(Consensus consensus) {
    return consensus == Consensus::ProofOfWork ? "Proof of Work" : "Proof of Stake";
}

bool commenceParZeros(const string& hash, int difficulte) {
    return hash.size() >= static_cast<size_t>(difficulte)
        && hash.compare(0, static_cast<size_t>(difficulte), string(difficulte, '0')) == 0;
}

struct Validateur {
    string nom;
    uint64_t mise;
};

struct Bloc {
    int index;
    long long horodatage;
    vector<string> transactions;
    string hashPrecedent;
    uint64_t nonce = 0;
    string validateur;
    string hash;

    Bloc(int numero, const vector<string>& tx, const string& precedent)
        : index(numero), horodatage(tempsActuelEnMillisecondes()),
          transactions(tx), hashPrecedent(precedent) {}

    string calculerHash() const {
        // La taille de chaque transaction est ajoutee pour eviter les concatenations ambigues.
        ostringstream contenu;
        contenu << index << '|' << horodatage << '|' << hashPrecedent << '|'
                << nonce << '|' << validateur << '|';
        for (const string& tx : transactions) {
            contenu << tx.size() << ':' << tx;
        }
        return sha256::hash(contenu.str());
    }

    void minerPoW(int difficulte) {
        nonce = 0;
        while (true) {
            hash = calculerHash();
            if (commenceParZeros(hash, difficulte)) return;
            ++nonce;
        }
    }
};

const Validateur& choisirValidateur(const vector<Validateur>& validateurs, mt19937_64& generateur) {
    uint64_t total = 0;
    for (const Validateur& validateur : validateurs) total += validateur.mise;
    if (total == 0) throw invalid_argument("Aucune mise positive n'est disponible.");

    uniform_int_distribution<uint64_t> tirage(1, total);
    const uint64_t numero = tirage(generateur);
    uint64_t cumul = 0;
    for (const Validateur& validateur : validateurs) {
        cumul += validateur.mise;
        if (numero <= cumul) return validateur;
    }
    return validateurs.back();
}

class Blockchain {
private:
    vector<Bloc> blocs;
    Consensus consensus;
    int difficulte;
    vector<Validateur> validateurs;
    mt19937_64 generateur;

public:
    Blockchain(Consensus methode, int niveau, const vector<Validateur>& listeValidateurs)
        : consensus(methode), difficulte(niveau), validateurs(listeValidateurs),
          generateur(random_device{}()) {
        if (difficulte < 1 || difficulte > 6) {
            throw invalid_argument("La difficulte PoW doit etre comprise entre 1 et 6.");
        }
        if (validateurs.empty()) {
            throw invalid_argument("Il faut fournir au moins un validateur.");
        }

        // Le bloc genesis sert de point de depart et n'est pas soumis au consensus.
        Bloc genesis(0, {"Creation de la chaine"}, "0");
        genesis.validateur = "GENESIS";
        genesis.hash = genesis.calculerHash();
        blocs.push_back(genesis);
    }

    void ajouterBloc(const vector<string>& transactions) {
        if (transactions.empty()) {
            throw invalid_argument("Un bloc doit contenir au moins une transaction.");
        }
        Bloc nouveau(static_cast<int>(blocs.size()), transactions, blocs.back().hash);

        if (consensus == Consensus::ProofOfWork) {
            nouveau.minerPoW(difficulte);
        } else {
            const Validateur& elu = choisirValidateur(validateurs, generateur);
            nouveau.validateur = elu.nom;
            // En PoS, on ne cherche pas un nonce : le validateur propose le bloc.
            nouveau.hash = nouveau.calculerHash();
        }
        blocs.push_back(nouveau);
    }

    bool estValide() const {
        if (blocs.empty()) return false;

        for (size_t i = 0; i < blocs.size(); ++i) {
            const Bloc& bloc = blocs[i];
            if (bloc.hash != bloc.calculerHash()) return false;

            if (i == 0) {
                if (bloc.hashPrecedent != "0") return false;
                continue;
            }

            if (bloc.hashPrecedent != blocs[i - 1].hash) return false;

            if (consensus == Consensus::ProofOfWork) {
                if (!commenceParZeros(bloc.hash, difficulte)) return false;
            } else {
                bool validateurConnuAvecMise = false;
                for (const Validateur& validateur : validateurs) {
                    if (validateur.nom == bloc.validateur && validateur.mise > 0) {
                        validateurConnuAvecMise = true;
                        break;
                    }
                }
                if (!validateurConnuAvecMise) return false;
            }
        }
        return true;
    }

    void modifierTransactionPourTest(size_t indexBloc, size_t indexTransaction,
                                     const string& nouveauTexte) {
        if (indexBloc >= blocs.size() || indexTransaction >= blocs[indexBloc].transactions.size()) {
            throw out_of_range("Indice de bloc ou de transaction invalide.");
        }
        // On change les donnees sans recalculer les hashes : l'integrite doit echouer.
        blocs[indexBloc].transactions[indexTransaction] = nouveauTexte;
    }

    const vector<Bloc>& obtenirBlocs() const { return blocs; }
    Consensus obtenirConsensus() const { return consensus; }
};

