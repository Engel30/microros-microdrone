#!/usr/bin/env bash
# Verifica che ogni riferimento a un FILE della documentazione punti a qualcosa
# che esiste davvero.
#
# Controlla due forme:
#   1. link markdown   [testo](percorso)
#   2. percorsi citati fra backtick   `docs/grounding/02-HARDWARE-BOM.md`
#
# La forma 2 e' la piu' usata in questo repo (prosa, non link cliccabili) ed e'
# anche quella che si rompe in silenzio: nessun renderer markdown la segnala.
#
# Per evitare falsi positivi si considera "riferimento a file" SOLO una stringa
# che finisce con un'estensione nota E che contiene una "/" (oppure ha la forma
# di un doc di grounding, es. 05-BRINGUP-QUICKSTART.md, citato come fratello).
# Restano quindi fuori — correttamente — i namespace di topic (`/drone_N/`), i
# tipi di messaggio ROS (`sensor_msgs/msg/Imu`), i nomi di componenti
# (`uros_interface/`), le librerie esterne (`esp-idf-lib/mpu6050`) e le menzioni
# in prosa di sorgenti e comandi (`drone_config.h`, `main.c`, `idf.py`).
#
# docs/archive/ e' escluso: i documenti archiviati citano la disposizione dei
# file dell'epoca (incluse spec poi eliminate) e non vanno riscritti.
#
# Risoluzione: valido se esiste relativo alla cartella del file che lo cita
# OPPURE relativo alla radice del repo (in questo repo la prosa usa percorsi
# radice-relativi, i link markdown percorsi file-relativi).
#
# Uso:  ./docs/check-links.sh        Exit 0 se tutti i riferimenti risolvono.

set -uo pipefail
cd "$(dirname "$0")/.." || exit 2
ROOT="$PWD"
errors=0
checked=0

EXT='md|csv|sh|py|c|h|json|yml|yaml|repos|meta|3mf|svg|png|txt|lock'

is_excluded() {
  case "$1" in
    ./build/*|./managed_components/*|./old/*|./.git/*) return 0 ;;
    ./ros2_ws/build/*|./ros2_ws/install/*|./ros2_ws/log/*) return 0 ;;
    ./ros2_ws/src/micro_ros_agent/*|./ros2_ws/src/micro_ros_msgs/*) return 0 ;;
    ./docs/archive/*) return 0 ;;
    *) return 1 ;;
  esac
}

check_ref() {
  local src="$1" ref="$2" dir
  case "$ref" in
    http*|mailto:*|\#*) return 0 ;;
    *...*) return 0 ;;                         # ellissi in prosa
  esac
  ref="${ref%%#*}"
  # solo cio' che ha l'aspetto di un percorso di file
  echo "$ref" | grep -qE "^[A-Za-z0-9._/-]+\.($EXT)$" || return 0
  # ...e che sia un percorso (contiene /) o un doc di grounding citato da fratello
  case "$ref" in
    */*) ;;
    [0-9][0-9]-[A-Z]*) ;;
    *) return 0 ;;
  esac
  dir="$(dirname "$src")"
  checked=$((checked + 1))
  [ -e "$dir/$ref" ] && return 0
  [ -e "$ROOT/$ref" ] && return 0
  printf '  ROTTO  %-58s -> %s\n' "${src#./}" "$ref"
  errors=$((errors + 1))
}

while IFS= read -r -d '' f; do
  is_excluded "$f" && continue
  while IFS= read -r ref; do
    [ -n "$ref" ] && check_ref "$f" "$ref"
  done < <(grep -oE '\]\([^)]+\)' "$f" 2>/dev/null | sed -E 's/^\]\(//; s/\)$//')
  while IFS= read -r ref; do
    [ -n "$ref" ] && check_ref "$f" "$ref"
  done < <(grep -oE '`[^`]+`' "$f" 2>/dev/null | tr -d '`')
done < <(find . -name '*.md' -type f -print0)

echo
if [ "$errors" -eq 0 ]; then
  echo "OK — $checked riferimenti a file verificati, nessuno rotto."
else
  echo "FALLITO — $errors riferimenti rotti su $checked verificati."
fi
exit $(( errors > 0 ? 1 : 0 ))
