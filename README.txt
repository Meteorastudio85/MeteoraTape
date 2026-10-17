METEORA TAPE - registratore a nastro VST3 (Windows) - grafica "J-47 TAPE"
========================================================================

COMPILARE SU GITHUB
1. Estrai lo zip. Crea un repository nuovo e premi "uploading an existing file".
2. Apri la cartella estratta, seleziona tutto (Ctrl+A, compresa la cartella .github) e trascinalo. Commit changes.
   Il workflow e' gia' al suo posto e trova da solo CMakeLists.txt.
   (Solo se nell'elenco non compare .github/workflows/build.yml: Add file -> Create new file con quel nome
   e incolla il file build.yml che trovi nello zip.)
3. Actions -> spunta verde -> scarica "MeteoraTape-VST3-Windows".
4. Chiudi la DAW, copia "Meteora Tape.vst3" in C:\Program Files\Common Files\VST3 (sostituisci il vecchio),
   riapri e rifai la scansione. Nome e codice sono gli stessi: i progetti salvati si riaprono.

COMANDI SULLA GRAFICA
- INPUT GAIN (-12 / +24 dB, 0 dB sul 5): spinge il segnale dentro il nastro. Oltre il 5 il nastro satura sempre di piu'.
- OUTPUT (-24 / +12 dB, 0 dB a meta' corsa): make-up gain.
  INPUT e OUTPUT sono collegati: girando INPUT, OUTPUT si muove al contrario della stessa quantita',
  cosi' aumenti la saturazione senza alzare il volume. Poi puoi ritoccare OUTPUT da solo.
  (Il collegamento vale quando giri INPUT col mouse; le automazioni della DAW muovono solo INPUT.)
- TAPE SPEED 7.5 / 15 / 30 IPS: pomello come gli altri due; clicca vicino al numero
  (o sul pomello per passare alla velocita' successiva).
- VU INPUT e OUTPUT: 0 VU = -18 dBFS. Il VU OUTPUT arrossisce quando il nastro satura:
  comincia sopra +6 VU sul nastro (circa 1.2% di distorsione) ed e' rosso pieno da +14 VU.

MISURE (15 IPS, 1 kHz a -18 dBFS, INPUT e OUTPUT collegati)
  INPUT +0 dB: distorsione 0.57%  | +6 dB: 1.22% | +12 dB: 2.93% (VU rosso al 75%)
  +18 dB: 8.45% | +24 dB: 18.6% (il nastro comprime: il volume scende di qualche dB, si ritocca con OUTPUT)

SENZA POMELLO (regolabili dalla DAW, automazioni comprese)
- Dry/Wet (100%), Hiss (spento; al 100% fruscio tipo Revox B77 MkIII).

LUCI E MOVIMENTO
- Le bobine girano in prospettiva quando il plugin lavora (la DAW manda audio e non e' in bypass):
  lente a 7.5, veloci a 30 IPS; partono e si fermano con un po' d'inerzia. Mozzi e riflessi restano fermi.
- Il logo Meteora in basso si accende come il filamento di una valvola, con luce morbida (si scalda in circa un secondo e mezzo),
  insieme ai VU, alle lampadine dei canali e ai piccoli VU in alto. Spento o in bypass: tutto scuro.
- Finestra: si apre a 1080 x 603, ridimensionabile dall'angolo o col clic destro (50-150%).
- Grafica piu' nitida: ingrandimento ripulito dagli artefatti, nitidezza a due scale senza aloni,
  contrasto locale leggero. Pomelli e bobine ruotano partendo da un'immagine a densita' doppia,
  cosi' la rotazione non impasta le zigrinature.

SUONO (invariato)
ToTape9 di Airwindows con la voce J37 per ogni velocita' (bump dei bassi, alti, hiss e THD come prima).
Misure a 15 IPS, 1 kHz: THD 0.57% a 0 VU, 1.22% a +6 VU, 2.93% a +12 VU.

LICENZE: vedi THIRD_PARTY_NOTICES.txt. JUCE 8 e' AGPLv3 o commerciale.
Se la build da' errori, manda il log.
