---
description: Chiude la sessione di lavoro: scrive il file sessione, aggiorna STATO.md e l'indice
---

Chiudi la sessione di lavoro corrente seguendo questa procedura. Non saltare passi e non aggiornare `docs/STATO.md` a mano fuori da qui.

## 1. Raccogli cosa è successo

```bash
git log --format='%h %ad %s' --date=short main..HEAD 2>/dev/null || git log --format='%h %ad %s' --date=short -10
git diff --stat HEAD~1 2>/dev/null
git status --short
```

Aggiungi quello che sai dalla conversazione e che il repo non registra: diagnosi fatte, test fisici, cose provate e scartate, osservazioni sull'hardware. **È la parte che vale di più**: il codice si rilegge, il ragionamento che ci ha portato no.

## 2. Scrivi il file sessione

`docs/sessions/YYYY-MM-DD-<slug>.md`, dove la data è oggi e lo slug descrive il lavoro (kebab-case, 2-5 parole).

**Se il file di oggi esiste già, appendi una sezione numerata** invece di crearne un altro: una sessione è un giorno di lavoro.

```markdown
# YYYY-MM-DD — <titolo>

**Branch:** `<branch>` · **Commit:** `<sha o range>`

## Fatto
Cosa è stato realizzato. File toccati, con il perché.

## Scoperto
Diagnosi, cause radice, vicoli ciechi. Anche quello che NON ha funzionato e
perché: fra sei mesi è l'informazione che serve.

## Aperto
Cosa resta irrisolto, cosa non è stato verificato e perché.

## Prossimo
Passi concreti per la prossima volta.
```

Regole: contenuto fedele, niente abbellimenti. Se una cosa non è stata verificata, scrivilo. I riferimenti ad altri documenti vanno in percorso completo dalla radice (`docs/grounding/05-BRINGUP-QUICKSTART.md`), non nudi.

## 3. Riscrivi `docs/STATO.md`

Riscrivilo **per intero**, non a toppe. Mantieni la struttura esistente e aggiorna:

- intestazione: data di oggi, link alla sessione appena scritta, branch corrente
- **In una riga** — dove siamo e qual è il prossimo passo
- **Dove siamo** — cosa funziona, con evidenza (commit, misura, test); cosa no
- **Blockers attivi** — rimuovi quelli risolti, aggiungi i nuovi, con impatto e soluzione ipotizzata
- **Prossimi 3 passi** — concreti e in ordine; se il passo 1 blocca il 3, dillo
- **Roadmap fasi** — solo se una fase ha cambiato stato
- **Ultime 3 sessioni** — link

Se il progetto resta fermo a lungo, aggiorna anche la checklist "alla ripresa, da verificare fisicamente".

**Invariante da rispettare:** lo stato volatile vive solo in `STATO.md`. Se stai per scrivere "Fase X in corso" dentro un doc di `grounding/`, fermati: va in `STATO.md`.

## 4. Aggiorna `docs/sessions/README.md`

Una riga in cima alla tabella: data, link, esito in poche parole.

## 5. Verifica

```bash
./docs/check-links.sh
```

Deve uscire con 0 errori. Se no, ripara prima di proporre il commit.

Se sono stati toccati componenti firmware, controlla che i `README.md` dentro `components/<nome>/` siano allineati.

## 6. Proponi il commit

Mostra `git status --short` e il messaggio proposto. **Non committare senza conferma.**

```
docs(sessione): <titolo sessione>

<2-4 righe: cosa è stato fatto e cosa è stato scoperto>

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

Chiudi riassumendo in tre righe dove siamo e qual è il primo passo della prossima volta.
