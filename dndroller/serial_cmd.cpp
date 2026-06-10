#include "serial_cmd.h"
#include "ui_buttons.h"
#include "ui_display.h"
#include "karma.h"
#include "config.h"
#include <Arduino.h>

extern int diceQuantity;
extern int selectedDiceIndex;
extern void rollDice();
extern int diceRolledSinceStart;
extern void updateAdvButton();
extern void ledApplyState();
extern AdvState advState;
extern int diceSides[7];

void printStatus() {
  const char* diceNames[] = {"D4","D6","D8","D10","D12","D20","D100"};
  Serial.println("──────────── STATUS ───────────────");
  Serial.printf("  Dado:      %s\n", selectedDiceIndex >= 0 ? diceNames[selectedDiceIndex] : "nessuno");
  Serial.printf("  Quantita:  %d\n", diceQuantity);
  Serial.printf("  Karma:     %s\n", useKarmicDice ? "ON" : "OFF");
  if (useKarmicDice) {
    float avg = getKarmaAverage();
    Serial.printf("  KarmaAvg:  %.3f  (0.5=neutro, >0.5=fortunato, <0.5=sfortunato)\n", avg);
    Serial.printf("  Lanci tracciati: %d/%d\n", karmaCount < KARMA_HISTORY ? karmaCount : KARMA_HISTORY, KARMA_HISTORY);
  }
  const char* advNames[] = {"NORMALE", "VANTAGGIO", "SVANTAGGIO"};
  Serial.printf("  Vantaggio: %s\n", advNames[advState]);
  Serial.printf("  DebugMode: %s\n", DEBUG_MODE ? "ON" : "OFF");
  Serial.println("────────────────────────────────────");
}

void handleSerialCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if      (cmd == "d4")   { btn0_action(); Serial.println("[CMD] Selezionato D4"); }
  else if (cmd == "d6")   { btn1_action(); Serial.println("[CMD] Selezionato D6"); }
  else if (cmd == "d8")   { btn2_action(); Serial.println("[CMD] Selezionato D8"); }
  else if (cmd == "d10")  { btn3_action(); Serial.println("[CMD] Selezionato D10"); }
  else if (cmd == "d12")  { btn4_action(); Serial.println("[CMD] Selezionato D12"); }
  else if (cmd == "d20")  { btn5_action(); Serial.println("[CMD] Selezionato D20"); }
  else if (cmd == "d100") { btn6_action(); Serial.println("[CMD] Selezionato D100"); }

  else if (cmd == "roll") {
    if (selectedDiceIndex == -1) Serial.println("[CMD] Errore: nessun dado selezionato");
    else { rollDice(); Serial.println("[CMD] Lancio eseguito"); }
  }

  else if (cmd.startsWith("qty ")) {
    int val = cmd.substring(4).toInt();
    if (val < 1 || val > 20) {
      Serial.printf("[CMD] Errore: quantita' deve essere 1-20 (ricevuto: %d)\n", val);
    } else {
      diceQuantity = val;
      updateQuantityDisplay();
      Serial.printf("[CMD] Quantita' impostata a %d\n", diceQuantity);
    }
  }

  else if (cmd == "adv") {
    if (selectedDiceIndex != 5) Serial.println("[CMD] Vantaggio disponibile solo per D20");
    else { advState = ADV_VANTAGGIO; updateAdvButton(); ledApplyState(); Serial.println("[CMD] Vantaggio attivato"); }
  }
  else if (cmd == "disadv") {
    if (selectedDiceIndex != 5) Serial.println("[CMD] Svantaggio disponibile solo per D20");
    else { advState = ADV_SVANTAGGIO; updateAdvButton(); ledApplyState(); Serial.println("[CMD] Svantaggio attivato"); }
  }
  else if (cmd == "normal") {
    advState = ADV_NORMAL; updateAdvButton(); ledApplyState();
    Serial.println("[CMD] Tiro normale");
  }

  else if (cmd == "karma on")  {
    useKarmicDice = true;
    Serial.println("[CMD] Karma ON");
  }
  else if (cmd == "karma off") {
    useKarmicDice = false;
    Serial.println("[CMD] Karma OFF");
  }
  else if (cmd == "karma reset") {
    karmaCount = 0;
    for (int i = 0; i < KARMA_HISTORY; i++) karmaHistory[i] = 0.5f;
    Serial.println("[CMD] Storia karmica resettata");
  }

  else if (cmd == "status") { printStatus(); }

  else if (cmd == "help" || cmd == "?") {
    Serial.println("Comandi disponibili:");
    Serial.println("  d4 d6 d8 d10 d12 d20 d100  — seleziona dado");
    Serial.println("  roll                        — esegui lancio");
    Serial.println("  qty <1-20>                  — imposta quantita'");
    Serial.println("  adv / disadv / normal       — vantaggio D20");
    Serial.println("  karma on/off/reset          — sistema karmico");
    Serial.println("  status                      — stato sistema");
  }

  else {
    Serial.printf("[CMD] Comando non riconosciuto: '%s' (digita 'help' per la lista)\n", cmd.c_str());
  }
}