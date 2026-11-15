
1. Structure générale d'une trame P1/P2

Chaque trame suit le format : [En-tête (3 octets)] + [Charge utile / Payload (variable)] + [Checksum CRC (1 octet)].
• Octet 0 (Direction) : 0x00 = Requête envoyée par l'interface (Maître) | 0x40 = Réponse de la pompe à chaleur (Esclave).
• Octet 1 (Adresse esclave) : 0x00 = Unité intérieure PAC | 0xF0 ou 0xF1 = Contrôleur externe (ex: module LAN / passerelle).
• Octet 2 (Type de paquet) : Identifie la nature des données (de 0x10 à 0x16 pour les capteurs et états courants).
• Dernier octet (CRC) : Contrôle de redondance cyclique calculé avec le polynôme 0xD9 (valeur initiale 0x00).

2. Exemples concrets de trames décodées


Exemple A : Lecture des températures (Paquet 0x11 - Réponse PAC)

Trame brute type : 40 00 11 23 80 30 00 08 40 1E 00 00 00 1A 80 15 00 08 40 00 00 00 00 [CRC]
Les températures sont codées sur 2 octets au format f8.8 (virgule fixe signée : diviser la valeur entière 16 bits par 256 pour obtenir les °C) :
• En-tête 40 00 11 : Réponse de la PAC (40 00) au paquet de températures (11).
• Octets 0-1 (23 80) : Température départ d'eau (LWT) → 0x2380 = 9088 / 256 = 35,5 °C.
• Octets 2-3 (30 00) : Température ballon ECS (DHW) → 0x3000 = 12288 / 256 = 48,0 °C.
• Octets 4-5 (08 40) : Température extérieure brute → 0x0840 = 2112 / 256 = 8,25 °C.
• Octets 6-7 (1E 00) : Température retour d'eau (RWT) → 0x1E00 = 7680 / 256 = 30,0 °C.
• Octets 10-11 (1A 80) : Température fluide frigorigène → 0x1A80 = 6784 / 256 = 26,5 °C.
• Octets 12-13 (15 00) : Température ambiante intérieure → 0x1500 = 5376 / 256 = 21,0 °C.

Exemple B : État de fonctionnement (Paquet 0x10 - Réponse PAC)

Trame brute type : 40 00 10 01 00 A1 01 30 00 0F 00 15 00 1A 00 00 00 00 00 00 09 04 [CRC]
• Octet 0 (01) : Bit 0 = 1 → Chauffage actif.
• Octet 2 (A1) : Bits de statut → Ballon ECS activé (bit 7), Zone principale active (bit 5), Mode chauffage (bit 0).
• Octet 4 (30) : Consigne ECS = 0x30 = 48 °C.
• Octet 8 (15) : Consigne ambiance = 0x15 = 21 °C.
• Octets 12-14 (00 00 00) : Code erreur actif (00 = aucune erreur).
• Octet 17 (00) : Bit 1 = 1 lorsque le dégivrage (Defrost) est en cours.
• Octet 18 (09) : 0x09 en binaire = 0000 1001 → Bit 3 (1) = Circulateur en marche, Bit 1 (0) = Résistance d'appoint arrêtée, Bit 0 (1) = Compresseur en marche.

3. Cartographie des principaux paquets de lecture (0x10 à 0xB8)


En-tête (Réponse)	Contenu du Payload	Détails des octets clés
40 00 10	États & Actionneurs	Marche/Arrêt zones, état compresseur, circulateur, résistance d'appoint, dégivrage, codes erreurs.
40 00 11	Températures	Départ eau (0-1), Ballon ECS (2-3), Extérieure (4-5), Retour eau (6-7), Réfrigérant (10-11), Ambiante (12-13).
40 00 12	Horloge & Modes	Synchronisation date/heure (dans la requête 00 00 12), indicateurs de fonctionnement ECS/PAC.
40 00 13	Débit & Versions	Octets 8-9 : Débit d'eau en 0,1 L/min (entier signé 16 bits). Octets 10-13 : Versions logicielles UI/UE.
40 00 14	Consignes Loi d'eau	Octets 15-16 : Consigne de départ d'eau calculée (Target LWT) en 0,1 °C.
40 00 16	Puissance & Courant	Octet 0 : Courant (en 0,1 A). Octet 1 : Puissance électrique absorbée (en 0,1 kW). Octets 6-7 : Puissance thermique produite.
40 00 B8	Compteurs d'énergie	Uniquement envoyé sur demande (00 00 B8) : kWh consommés/produits, heures de fonctionnement, nombre de démarrages compresseur.


4. Paquets d'écriture (Commandes vers la PAC)

Pour modifier une consigne (en émulant un contrôleur externe sur l'adresse 0xF0), l'interface maître envoie des requêtes 00 F0 35, 00 F0 36 ou 00 F0 3A contenant des paires [Numéro de paramètre (2 octets Little Endian)] + [Valeur] :
• Paquet 0x35 (Interrupteurs ON/OFF) : Paramètre 0x4000 (Marche/Arrêt ECS), 0x4800 (Mode Boost ECS), 0x0300 (Mode Silencieux).
• Paquet 0x36 (Consignes en 0,1 °C, Little Endian) : Paramètre 0x0300 (Consigne ECS, ex: D6 01 = 0x01D6 = 470 = 47,0 °C), 0x0600 (Consigne départ eau fixe), 0x0800 (Décalage loi d'eau).

Dépôts directs avec les tables complètes

Vous pouvez consulter les tableaux complets (octet par octet) directement dans ces deux fichiers markdown issus des travaux de reverse engineering de Arnold Niessen :
• Table complète des paquets de lecture (Payload-data-read.md)
• Table complète des paquets d'écriture (Payload-data-write.md)



## Important Caveat: Daikin Uses Multiple Formats

While packet 0x11 uses standard Q8.8 (f8.8), Daikin mixes several other temperature formats across different packets and generations on the P1/P2 bus:
• Q8.8 (f8.8): Used in packet 0x11 (sensor temperatures). Example: 0x1580 = 5504 / 256 = 21.5 °C.
• Daikin BCD-like (f8/8): Used on some older Altherma/split models where the first byte is the integer part and the second byte is the decimal digit (0x21 0x05 = 21.5 °C).
• Integer × 10 (u16div10 / s16div10): Used in packet 0x14 (target LWT) and write packets (0x36), where the signed/unsigned 16-bit integer is divided by 10 (e.g., 0x01B6 = 438 / 10 = 43.8 °C).
• 7-bit Signed Float (sfp7): Used in field settings packets (1 sign bit, 4 mantissa bits, 2 exponent bits).