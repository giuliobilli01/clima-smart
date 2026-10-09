# Clima Smart

Progetto per l'esame di Laboratorio di Making: un dispositivo con ESP32-S3 che rende smart un vecchio condizionatore, senza sostituirlo.

- **Comandi a infrarossi**: invia accensione, modo, temperatura e ventola come il telecomando originale. Il protocollo si impara dal telecomando, quindi funziona con i condizionatori supportati dalla libreria IRremoteESP8266.
- **Telegram e pagina web**: si comanda da un bot Telegram, da ovunque, e da una pagina web sulla rete di casa, protetta da password.
- **Stato aggiornato**: ascolta il telecomando originale, e un fototransistor sul LED di stato del condizionatore corregge l'accensione se un comando si perde.
- **Automazioni sulla scheda**: spegne il condizionatore se la porta resta aperta, lo accende se la stanza è troppo fredda o troppo calda, e lo scrive in chat. Funzionano anche senza internet.

Il codice del dispositivo è in [`firmware/`](firmware) (ESP-IDF). In [`testbed/`](testbed) c'è il test-bed per la demo: un secondo ESP32-S3 che fa la parte del condizionatore e mostra lo stato su un display LCD.
