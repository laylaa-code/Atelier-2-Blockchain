"""Graphiques de l'atelier 2, a partir des CSV produits par le programme C++.
Les SVG se creent avec Python seul. Si matplotlib est installe, des PNG
sont aussi produits pour le rapport et les diapositives.
"""
import csv
from pathlib import Path
from html import escape

RACINE = Path(__file__).resolve().parent
DONNEES = RACINE / "resultats"
DOSSIER = RACINE / "graphiques"
DOSSIER.mkdir(exist_ok=True)

with (DONNEES / "resume.csv").open(encoding="utf-8", newline="") as f:
    RESUME = list(csv.DictReader(f))
with (DONNEES / "comparaison_detection.csv").open(encoding="utf-8", newline="") as f:
    DETECTION = list(csv.DictReader(f))

ATTAQUES = ["51_pourcent", "selfish_mining", "block_withholding", "sybil"]
LIBELLES = ["51 %", "Selfish mining", "Block withholding", "Sybil"]
ETATS = ["reference", "attaquee", "protegee"]
NOMS = ["Reference", "Attaquee", "Protegee"]
COULEURS = ["#8395a7", "#d95d39", "#397d82"]


def valeur(attaque, etat, colonne, detecteur="adaptatif"):
    for ligne in RESUME:
        if ligne["attaque"] == attaque and ligne["etat"] == etat and ligne["detecteur"] == detecteur:
            return float(ligne[colonne])
    raise ValueError("Ligne manquante : " + attaque + " " + etat)


def graphique_barres(nom_fichier, titre, colonne, unite="", etats=ETATS):
    groupes = [[valeur(a, e, colonne) for e in etats] for a in ATTAQUES]
    plus_grand = max(max(g) for g in groupes)
    plafond = max(1, plus_grand * 1.18)
    largeur, hauteur = 920, 490
    gauche, droite, haut, bas = 86, 35, 75, 80
    hgraph = hauteur - haut - bas
    wgraph = largeur - gauche - droite
    couleur_etat = {e: COULEURS[ETATS.index(e)] for e in etats}
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{largeur}" height="{hauteur}" viewBox="0 0 {largeur} {hauteur}">',
           '<rect width="100%" height="100%" fill="white"/>',
           f'<text x="{largeur/2}" y="36" text-anchor="middle" font-family="Arial" font-size="21" fill="#18242c">{escape(titre)}</text>']
    for j in range(6):
        v = plafond * j / 5
        y = haut + hgraph * (1 - j / 5)
        svg.append(f'<line x1="{gauche}" y1="{y:.2f}" x2="{largeur-droite}" y2="{y:.2f}" stroke="#e2e6eb"/>')
        svg.append(f'<text x="{gauche-12}" y="{y+5:.2f}" text-anchor="end" font-family="Arial" font-size="12" fill="#56636c">{v:.0f}</text>')
    for i, label in enumerate(LIBELLES):
        cx = gauche + (i+0.5) * wgraph / len(LIBELLES)
        svg.append(f'<text x="{cx}" y="{hauteur-bas+25}" text-anchor="middle" font-family="Arial" font-size="14" fill="#18242c">{escape(label)}</text>')
        nbar = len(etats)
        bar_width = 40 if nbar == 3 else 50
        for j, etat in enumerate(etats):
            v = groupes[i][j]
            h = hgraph * v / plafond
            x = cx + (j - (nbar-1)/2) * (bar_width+5) - bar_width/2
            y = haut + hgraph - h
            svg.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_width}" height="{h:.1f}" fill="{couleur_etat[etat]}"/>')
            if v > 0:
                svg.append(f'<text x="{x+bar_width/2:.1f}" y="{max(haut+8,y-6):.1f}" text-anchor="middle" font-family="Arial" font-size="11" fill="#22313a">{v:.1f}</text>')
    for j, etat in enumerate(etats):
        lx = gauche + j * 195
        svg.append(f'<rect x="{lx}" y="{hauteur-26}" width="14" height="13" fill="{couleur_etat[etat]}"/>')
        svg.append(f'<text x="{lx+20}" y="{hauteur-15}" font-family="Arial" font-size="13">{escape(NOMS[ETATS.index(etat)])}</text>')
    if unite:
        svg.append(f'<text x="{gauche}" y="{haut-17}" font-family="Arial" font-size="12" fill="#56636c">{escape(unite)}</text>')
    svg.append('</svg>')
    (DOSSIER / nom_fichier).write_text("\n".join(svg), encoding="utf-8")


graphique_barres("01_taux_succes.svg", "Reussite des attaques selon la protection", "taux_succes_pourcent", "% des essais")
graphique_barres("02_pertes.svg", "Pertes simulees (comparaison pour chaque attaque)", "perte_moyenne_unites", "Unites propres a chaque scenario")
graphique_barres("03_surcout.svg", "Surcout des messages de controle", "surcout_controle_pourcent", "% des messages normaux", ["protegee"])

# Detection : comparaison de la precision avec une regle fixe ou adaptee.
X = [d for d in DETECTION if d["attaque"] in ATTAQUES]
if X:
    W, H, L, T, B = 920, 450, 85, 75, 78
    max_h = H - T - B
    px = (W - L - 35) / 4
    morceaux = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
                '<rect width="100%" height="100%" fill="white"/>',
                f'<text x="{W/2}" y="36" text-anchor="middle" font-family="Arial" font-size="21">Precision de la detection : seuil fixe et adaptatif</text>']
    for v in [0, 25, 50, 75, 100]:
        y = T + max_h*(1-v/100)
        morceaux += [f'<line x1="{L}" x2="{W-35}" y1="{y}" y2="{y}" stroke="#e2e6eb"/>',
                     f'<text x="{L-12}" y="{y+5}" text-anchor="end" font-family="Arial" font-size="12">{v}%</text>']
    for i, attaque in enumerate(ATTAQUES):
        cx = L + px*(i+.5)
        for j, methode in enumerate(["fixe", "adaptatif"]):
            val = float(next(x["precision_pourcent"] for x in X if x["attaque"] == attaque and x["detecteur"] == methode))
            h = val*max_h/100
            x = cx - 35 + j*36
            y = T + max_h - h
            couleur = "#8395a7" if j == 0 else "#397d82"
            morceaux += [f'<rect x="{x}" y="{y}" width="32" height="{h}" fill="{couleur}"/>',
                         f'<text x="{x+16}" y="{max(T+8,y-7)}" text-anchor="middle" font-family="Arial" font-size="11">{val:.0f}</text>']
        morceaux.append(f'<text x="{cx}" y="{H-B+24}" text-anchor="middle" font-family="Arial" font-size="14">{escape(LIBELLES[i])}</text>')
    morceaux += [f'<rect x="{L}" y="{H-27}" width="13" height="13" fill="#8395a7"/>',
                 f'<text x="{L+20}" y="{H-16}" font-family="Arial" font-size="13">Seuil fixe</text>',
                 f'<rect x="{L+160}" y="{H-27}" width="13" height="13" fill="#397d82"/>',
                 f'<text x="{L+180}" y="{H-16}" font-family="Arial" font-size="13">Seuil adaptatif</text>', '</svg>']
    (DOSSIER / "04_precision_detection.svg").write_text("\n".join(morceaux), encoding="utf-8")

# Des PNG sont crees seulement si matplotlib est disponible.
try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    print("SVG crees. Matplotlib absent : pas de PNG supplementaires.")
else:
    fig, ax = plt.subplots(figsize=(10.2, 5.5))
    x = np.arange(4)
    for j, etat in enumerate(ETATS):
        v = [valeur(a,etat,"taux_succes_pourcent") for a in ATTAQUES]
        ax.bar(x+(j-1)*0.24, v, 0.22, label=NOMS[j], color=COULEURS[j])
    ax.set_xticks(x, LIBELLES)
    ax.set_ylabel("Essais reussis (%)")
    ax.set_ylim(0, 115)
    ax.legend()
    ax.set_title("Reussite des attaques : avant et apres protection")
    fig.tight_layout(); fig.savefig(DOSSIER/"01_taux_succes.png", dpi=150); plt.close(fig)

    fig, ax = plt.subplots(figsize=(10.2, 5.5))
    for j, etat in enumerate(ETATS):
        v = [valeur(a,etat,"perte_moyenne_unites") for a in ATTAQUES]
        ax.bar(x+(j-1)*0.24, v, 0.22, label=NOMS[j], color=COULEURS[j])
    ax.set_xticks(x, LIBELLES); ax.set_ylabel("Pertes moyennes (unites du scenario)")
    ax.set_title("Pertes simulees : valeurs comparables au sein d'une meme attaque")
    ax.legend(); fig.tight_layout(); fig.savefig(DOSSIER/"02_pertes.png", dpi=150); plt.close(fig)

    fig, ax = plt.subplots(figsize=(10.2, 5.5))
    v = [valeur(a,"protegee","surcout_controle_pourcent") for a in ATTAQUES]
    ax.bar(x, v, 0.54, color=COULEURS[2])
    ax.set_xticks(x, LIBELLES); ax.set_ylabel("Messages de controle supplementaires (%)")
    ax.set_title("Cout reseau des contre-mesures")
    fig.tight_layout(); fig.savefig(DOSSIER/"03_surcout.png", dpi=150); plt.close(fig)

    fig, ax = plt.subplots(figsize=(10.2, 5.5))
    for j, methode in enumerate(["fixe", "adaptatif"]):
        v = [float(next(d["precision_pourcent"] for d in DETECTION if d["attaque"]==a and d["detecteur"]==methode)) for a in ATTAQUES]
        ax.bar(x+(j-.5)*0.28, v, 0.26, color=[COULEURS[0], COULEURS[2]][j], label=methode)
    ax.set_xticks(x, LIBELLES); ax.set_ylim(0, 110)
    ax.set_ylabel("Precision (%)"); ax.legend(); ax.set_title("Detection par regles : fixe contre adaptatif")
    fig.tight_layout(); fig.savefig(DOSSIER/"04_precision_detection.png", dpi=150); plt.close(fig)
    print("SVG et PNG generes dans graphiques/.")
