#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

FT_FILE_DEFAULT="$ROOT_DIR/artifact/experiments/models/FT/PHASE_PAND_REUSE.ft"
FT_FILE="${1:-$FT_FILE_DEFAULT}"

# 这些环境变量控制 IFUN 类型、运行模式以及 FIG 的 splitting 参数。
IFUN_KIND="${IFUN_KIND:-STR}"   # STR | MCS | TIME | PHYS | TIMEPHYS | PWSTR | T3 | M3P | N4 | N4P | N4Q | N4A | N4AH | N4AN | N5C | N5P | NONE
MODE="${MODE:-isplit}"          # isplit | cmc | trace
ENGINE="${ENGINE:-restart}"     # restart | sfe | nosplit
THR_TECH="${THR_TECH:-hyb}"     # hyb | es | ams | fix | smc
# Optional: precomputed thresholds in FIG --thresholds-adhoc format
# (e.g. "70:4,80:4,90:4"). When non-empty, FIG skips SMC/hyb rebuild.
THRESHOLDS_ADHOC="${THRESHOLDS_ADHOC:-}"
GLOBAL_EFFORT="${GLOBAL_EFFORT:-3}"
STOP_TIME="${STOP_TIME:-30s}"
TIMEOUT_S="${TIMEOUT_S:-}"
RNG_SEED="${RNG_SEED:-1}"
USE_FT_ISPLIT="${USE_FT_ISPLIT:-1}"  # 0: use --adhoc without --ft; 1: use --ft + --acomp (default)
FIG_TIMEOUT="${FIG_TIMEOUT:-600}"  # wall-clock seconds before killing fig (default 10 min)
FIG_VERBOSE="${FIG_VERBOSE:-1}"
TRANSIENT_HORIZON="${TRANSIENT_HORIZON:-30}"
FIG_ISPLIT_DIAG="${FIG_ISPLIT_DIAG:-0}"
BATS_TARGET_P="${BATS_TARGET_P:-}"
PROPERTY_EXECUTION_MODE="${PROPERTY_EXECUTION_MODE:-single}"
case "$PROPERTY_EXECUTION_MODE" in
  single|legacy_double) ;;
  *)
    echo "ERROR: PROPERTY_EXECUTION_MODE must be single or legacy_double" >&2
    exit 2
    ;;
esac
PHYS_K="${PHYS_K:-10}"
PHYS_BETA="${PHYS_BETA:-1.0}"
PHYS_GAMMA="${PHYS_GAMMA:-1.0}"
PAND_ETA="${PAND_ETA:-1.0}"
M_MAX="${M_MAX:-5}"
FORCE_SPLIT="${FORCE_SPLIT:-0}"  # 1: force split formula even with ENGINE=sfe
N4_K="${N4_K:-500}"
N4_SCALE="${N4_SCALE:-1000}"
N4_D_SCALE="${N4_D_SCALE:-100}"
N4_MISSION_TIME="${N4_MISSION_TIME:-}"
N4A_ALPHA_MIN="${N4A_ALPHA_MIN:-0.1}"
N4A_RARE_LOW="${N4A_RARE_LOW:-3}"
N4A_RARE_HIGH="${N4A_RARE_HIGH:-8}"
N4A_HETERO_LOW="${N4A_HETERO_LOW:-3}"
N4A_HETERO_HIGH="${N4A_HETERO_HIGH:-30}"
N4A_BOTTLENECK_LOW="${N4A_BOTTLENECK_LOW:-0.40}"
N4A_BOTTLENECK_HIGH="${N4A_BOTTLENECK_HIGH:-0.70}"
N4A_INF_MULT="${N4A_INF_MULT:-10}"
N4A_FIXED_ALPHA_FLAG="${N4A_FIXED_ALPHA_FLAG:-0}"
N4A_FIXED_ALPHA_VALUE="${N4A_FIXED_ALPHA_VALUE:-0}"
ARDI_ALPHA_LAW="${ARDI_ALPHA_LAW:-inverse_low_temp}"
ARDI_ALPHA_MIN="${ARDI_ALPHA_MIN:-0.10}"
ARDI_ALPHA_MAX="${ARDI_ALPHA_MAX:-0.25}"
ARDI_FIXED_ALPHA_FLAG="${ARDI_FIXED_ALPHA_FLAG:-0}"
ARDI_FIXED_ALPHA_VALUE="${ARDI_FIXED_ALPHA_VALUE:-0.10}"
CLF_RISK_THRESHOLD="${CLF_RISK_THRESHOLD:-}"
N5C_K="${N5C_K:-$N4_K}"
N5C_SCALE="${N5C_SCALE:-$N4_SCALE}"
N5C_D_SCALE="${N5C_D_SCALE:-$N4_D_SCALE}"
N5C_MISSION_TIME="${N5C_MISSION_TIME:-$N4_MISSION_TIME}"
N5C_INF_MULT="${N5C_INF_MULT:-$N4A_INF_MULT}"
N5C_ORDER_FACTOR="${N5C_ORDER_FACTOR:-0}"
N5C_SHARED_GUARD="${N5C_SHARED_GUARD:-0}"
N5C_EPS_PROB="${N5C_EPS_PROB:-1e-38}"
N5P_K="${N5P_K:-$N4_K}"
N5P_SCALE="${N5P_SCALE:-$N4_SCALE}"
N5P_D_SCALE="${N5P_D_SCALE:-$N4_D_SCALE}"
N5P_MISSION_TIME="${N5P_MISSION_TIME:-$N4_MISSION_TIME}"
N5P_INF_MULT="${N5P_INF_MULT:-$N4A_INF_MULT}"
N5P_L="${N5P_L:-64}"
N5P_TAU="${N5P_TAU:-0.5}"
N5P_W="${N5P_W:-6.0}"
N5P_TAU_W="${N5P_TAU_W:-1.0}"
N5P_ETA="${N5P_ETA:-0.5}"
N5P_B_CAP="${N5P_B_CAP:-0.05}"
N5P_N_EFF_FULL="${N5P_N_EFF_FULL:-4.0}"
N5P_MODEL_ID="${N5P_MODEL_ID:-}"

FIG_BIN="${FIG_BIN_OVERRIDE:-$ROOT_DIR/artifact/setup/fig/build/fig_files_release_gcc/fig}"
KOMPILATOR="$ROOT_DIR/artifact/setup/fig/etc/kepler_parser/kompilator.py"
IOSA_PY="$ROOT_DIR/artifact/setup/fig/etc/kepler_parser/iosa.py"
KEPLER_PY="$ROOT_DIR/artifact/setup/fig/etc/kepler_parser/kepler.py"
COMPILE_CACHE_ROOT="${COMPILE_CACHE_ROOT:-$ROOT_DIR/artifact/experiments/outputs/.iosa_cache}"

cache_hash_file() {
  local file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  else
    python3 - "$file" <<'PY'
import hashlib, sys
path = sys.argv[1]
h = hashlib.sha256()
with open(path, 'rb') as f:
    while True:
        chunk = f.read(1024 * 1024)
        if not chunk:
            break
        h.update(chunk)
print(h.hexdigest())
PY
  fi
}

cache_key_string() {
  printf '%s\n' \
    "cache_schema=single_property_v2" \
    "property_execution_mode=$PROPERTY_EXECUTION_MODE" \
    "ft=$(cache_hash_file "$FT_FILE")" \
    "kompilator=$(cache_hash_file "$KOMPILATOR")" \
    "iosa=$(cache_hash_file "$IOSA_PY")" \
    "kepler=$(cache_hash_file "$KEPLER_PY")" \
    "ifun=$IFUN_KIND" \
    "transient=$TRANSIENT_HORIZON" \
    "phys_k=$PHYS_K" \
    "phys_beta=$PHYS_BETA" \
    "phys_gamma=$PHYS_GAMMA" \
    "pand_eta=$PAND_ETA" \
    "n4_k=$N4_K" \
    "n4_scale=$N4_SCALE" \
    "n4_d_scale=$N4_D_SCALE" \
    "n4_mission_time=$N4_MISSION_TIME" \
    "n4a_alpha_min=$N4A_ALPHA_MIN" \
    "n4a_rare_low=$N4A_RARE_LOW" \
    "n4a_rare_high=$N4A_RARE_HIGH" \
    "n4a_hetero_low=$N4A_HETERO_LOW" \
    "n4a_hetero_high=$N4A_HETERO_HIGH" \
    "n4a_bottleneck_low=$N4A_BOTTLENECK_LOW" \
    "n4a_bottleneck_high=$N4A_BOTTLENECK_HIGH" \
    "n4a_inf_mult=$N4A_INF_MULT" \
    "n4a_fixed_alpha_flag=$N4A_FIXED_ALPHA_FLAG" \
    "n4a_fixed_alpha_value=$N4A_FIXED_ALPHA_VALUE" \
    "ardi_alpha_law=$ARDI_ALPHA_LAW" \
    "ardi_alpha_min=$ARDI_ALPHA_MIN" \
    "ardi_alpha_max=$ARDI_ALPHA_MAX" \
    "ardi_fixed_alpha_flag=$ARDI_FIXED_ALPHA_FLAG" \
    "ardi_fixed_alpha_value=$ARDI_FIXED_ALPHA_VALUE" \
    "clf_risk_threshold=$CLF_RISK_THRESHOLD" \
    "n5c_k=$N5C_K" \
    "n5c_scale=$N5C_SCALE" \
    "n5c_d_scale=$N5C_D_SCALE" \
    "n5c_mission_time=$N5C_MISSION_TIME" \
    "n5c_inf_mult=$N5C_INF_MULT" \
    "n5c_order_factor=$N5C_ORDER_FACTOR" \
    "n5c_shared_guard=$N5C_SHARED_GUARD" \
    "n5c_eps_prob=$N5C_EPS_PROB" \
    "n5p_k=$N5P_K" \
    "n5p_scale=$N5P_SCALE" \
    "n5p_d_scale=$N5P_D_SCALE" \
    "n5p_mission_time=$N5P_MISSION_TIME" \
    "n5p_inf_mult=$N5P_INF_MULT" \
    "n5p_l=$N5P_L" \
    "n5p_tau=$N5P_TAU" \
    "n5p_w=$N5P_W" \
    "n5p_tau_w=$N5P_TAU_W" \
    "n5p_eta=$N5P_ETA" \
    "n5p_b_cap=$N5P_B_CAP" \
    "n5p_n_eff_full=$N5P_N_EFF_FULL" \
    "split_m_max=${SPLIT_M_MAX:-}"
}

cache_key_hash() {
  if command -v sha256sum >/dev/null 2>&1; then
    cache_key_string | sha256sum | awk '{print $1}'
  else
    cache_key_string | python3 <<'PY'
import hashlib, sys
payload = sys.stdin.read().encode('utf-8')
print(hashlib.sha256(payload).hexdigest())
PY
  fi
}

strip_embedded_properties() {
  local src="$1"
  local dst="$2"
  awk '
    /^properties$/ { in_properties=1; next }
    /^endproperties$/ { in_properties=0; next }
    !in_properties { print }
  ' "$src" > "$dst"
}

populate_cached_build() {
  local cache_dir="$1"
  local cache_model="$cache_dir/model.sa"
  local cache_prop="$cache_dir/property.prop"
  local lock_dir="$cache_dir.lock"

  if [ -f "$cache_model" ] && [ -f "$cache_prop" ]; then
    cp "$cache_model" "$IOSA_FILE"
    cp "$cache_prop" "$PROP_FILE"
    return 0
  fi

  mkdir -p "$COMPILE_CACHE_ROOT"
  while ! mkdir "$lock_dir" 2>/dev/null; do
    if [ -f "$cache_model" ] && [ -f "$cache_prop" ]; then
      cp "$cache_model" "$IOSA_FILE"
      cp "$cache_prop" "$PROP_FILE"
      return 0
    fi
    sleep 1
  done

  if [ -f "$cache_model" ] && [ -f "$cache_prop" ]; then
    rmdir "$lock_dir" 2>/dev/null || true
    cp "$cache_model" "$IOSA_FILE"
    cp "$cache_prop" "$PROP_FILE"
    return 0
  fi

  local tmp_cache_dir
  tmp_cache_dir="$(mktemp -d "$COMPILE_CACHE_ROOT/.build_XXXXXX")"
  local tmp_model="$tmp_cache_dir/model.sa"
  local tmp_model_stripped="$tmp_cache_dir/model.no_property.sa"
  local tmp_prop="$tmp_cache_dir/property.prop"

  echo "STEP 1/3: compile IOSA"
  if command -v conda >/dev/null 2>&1; then
    conda run -n 3.10 python "${KOMP_ARGS[@]/$IOSA_FILE/$tmp_model}"
  else
    python3 "${KOMP_ARGS[@]/$IOSA_FILE/$tmp_model}"
  fi

  echo "STEP 2/3: extract property"
  awk '/^properties$/{flag=1;next}/^endproperties$/{flag=0}flag' "$tmp_model" > "$tmp_prop"
  if ! grep -Eq '^[[:space:]]*P\(' "$tmp_prop"; then
    cat > "$tmp_prop" <<'EOF'
P( timeout<2 U (SysFail_count!=0) )
EOF
  fi
  strip_embedded_properties "$tmp_model" "$tmp_model_stripped"

  mkdir -p "$cache_dir"
  if [ "$PROPERTY_EXECUTION_MODE" = "single" ]; then
    cp "$tmp_model_stripped" "$cache_model"
  else
    echo "INFO: legacy_double keeps embedded property and also passes property.prop"
    cp "$tmp_model" "$cache_model"
  fi
  cp "$tmp_prop" "$cache_prop"
  cp "$cache_model" "$IOSA_FILE"
  cp "$cache_prop" "$PROP_FILE"
  rm -rf "$tmp_cache_dir"
  rmdir "$lock_dir" 2>/dev/null || true
}

if ! [ -f "$FT_FILE" ]; then
  echo "ERROR: FT file not found: $FT_FILE" >&2
  exit 2
fi

if ! [ -f "$KOMPILATOR" ]; then
  echo "ERROR: kompilator.py not found: $KOMPILATOR" >&2
  exit 2
fi

FIG_CLI_SRC="$ROOT_DIR/artifact/setup/fig/src/fig_cli.cpp"
if ! [ -x "$FIG_BIN" ]; then
  echo "INFO: fig binary not found, building..." >&2
  PATH=/usr/bin:/bin:$PATH bash "$ROOT_DIR/artifact/setup/fig/build_fig.sh"
elif [ -f "$FIG_CLI_SRC" ] && [ "$FIG_CLI_SRC" -nt "$FIG_BIN" ]; then
  echo "INFO: fig sources newer than binary, rebuilding..." >&2
  PATH=/usr/bin:/bin:$PATH bash "$ROOT_DIR/artifact/setup/fig/build_fig.sh"
fi

if ! [ -x "$FIG_BIN" ]; then
  echo "ERROR: fig binary not found/executable: $FIG_BIN" >&2
  exit 2
fi

PHASE_TMP_ROOT="${PHASE_PAND_TMP_ROOT:-/tmp}"
mkdir -p "$PHASE_TMP_ROOT"
PHASE_TASK_ID="${PHASE_TASK_ID:-task}"
PHASE_TASK_ID="$(printf '%s' "$PHASE_TASK_ID" | tr -c 'A-Za-z0-9_.-' '_')"
OUT_DIR="$(mktemp -d "$PHASE_TMP_ROOT/phase_pand_reuse_${PHASE_TASK_ID}_$(date +%Y%m%d_%H%M%S)_XXXXXX")"

IOSA_FILE="$OUT_DIR/model.sa"
PROP_FILE="$OUT_DIR/property.prop"
OUT_FILE="$OUT_DIR/fig.out"
ERR_FILE="$OUT_DIR/fig.err"
SPLIT_M_MAX=""
if [ "$ENGINE" = "restart" ] && { [ "$IFUN_KIND" = "TIMEPHYS" ] || [ "$IFUN_KIND" = "PHYS" ]; }; then
  SPLIT_M_MAX="${M_MAX:-}"
fi
if [ -z "$SPLIT_M_MAX" ] && [ "$ENGINE" = "restart" ] && { [ "$IFUN_KIND" = "TIMEPHYS" ] || [ "$IFUN_KIND" = "PHYS" ]; }; then
  SPLIT_M_MAX="$GLOBAL_EFFORT"
fi

if [ "$FORCE_SPLIT" = "1" ] && { [ "$IFUN_KIND" = "TIMEPHYS" ] || [ "$IFUN_KIND" = "PHYS" ]; }; then
  if [ -z "$SPLIT_M_MAX" ]; then
    SPLIT_M_MAX="${M_MAX:-}"
  fi
  if [ -z "$SPLIT_M_MAX" ]; then
    SPLIT_M_MAX="$GLOBAL_EFFORT"
  fi
fi
THRESHOLD_COUNT=0
if [ -n "$THRESHOLDS_ADHOC" ]; then
  THRESHOLD_COUNT="$(printf '%s' "$THRESHOLDS_ADHOC" | awk -F',' '{print NF}')"
fi
M5_RESTART_MIN_THRESHOLDS="${M5_RESTART_MIN_THRESHOLDS:-$((GLOBAL_EFFORT * GLOBAL_EFFORT + 1))}"
if [ "$ENGINE" = "restart" ] && { [ "$IFUN_KIND" = "TIMEPHYS" ] || [ "$IFUN_KIND" = "PHYS" ]; } && [ "$THRESHOLD_COUNT" -lt "$M5_RESTART_MIN_THRESHOLDS" ] 2>/dev/null && [ -n "$THRESHOLDS_ADHOC" ]; then
  ENGINE="sfe"
  SPLIT_M_MAX=""
fi
KOMP_ARGS=("$KOMPILATOR" "$FT_FILE" "$IOSA_FILE" -f -t "$TRANSIENT_HORIZON" -i "$IFUN_KIND" --phys-k "$PHYS_K" --phys-beta "$PHYS_BETA" --phys-gamma "$PHYS_GAMMA" --pand-eta "$PAND_ETA")

if [ -n "$SPLIT_M_MAX" ] && [ "$SPLIT_M_MAX" -gt 1 ] 2>/dev/null && [ "$IFUN_KIND" = "TIMEPHYS" ]; then
  KOMP_ARGS+=(--split-mmax "$SPLIT_M_MAX")
fi

echo "START FT_FILE=$FT_FILE IFUN_KIND=$IFUN_KIND MODE=$MODE ENGINE=$ENGINE FORCE_SPLIT=$FORCE_SPLIT RNG_SEED=$RNG_SEED PROPERTY_EXECUTION_MODE=$PROPERTY_EXECUTION_MODE"
echo "OUT_DIR: $OUT_DIR"
CACHE_KEY="$(cache_key_hash)"
CACHE_DIR="$COMPILE_CACHE_ROOT/$CACHE_KEY"
if [ -f "$CACHE_DIR/model.sa" ] && [ -f "$CACHE_DIR/property.prop" ]; then
  echo "STEP 1/3: reuse cached IOSA"
  cp "$CACHE_DIR/model.sa" "$IOSA_FILE"
  cp "$CACHE_DIR/property.prop" "$PROP_FILE"
else
  populate_cached_build "$CACHE_DIR"
fi

if [ "$PROPERTY_EXECUTION_MODE" = "single" ]; then
  if grep -Eq '^properties$|^endproperties$' "$IOSA_FILE"; then
    echo "ERROR: IOSA file still contains embedded properties while property.prop is also passed" >&2
    exit 4
  fi
else
  if ! grep -Eq '^properties$|^endproperties$' "$IOSA_FILE"; then
    echo "ERROR: legacy_double requires one embedded property plus property.prop" >&2
    exit 4
  fi
fi
if [ "$(grep -Ec '^[[:space:]]*P\(' "$PROP_FILE" || true)" -ne 1 ]; then
  echo "ERROR: property.prop must contain exactly one P(...) property" >&2
  exit 4
fi

FIG_ARGS=("$FIG_BIN")
if [ "$FIG_VERBOSE" != "0" ]; then
  FIG_ARGS+=(--parlare "$FIG_VERBOSE")
fi
FIG_ARGS+=(--rng-seed "$RNG_SEED")

if [ -n "$TIMEOUT_S" ]; then
  FIG_ARGS+=(--timeout "$TIMEOUT_S")
fi

case "$MODE" in
  trace)
    FIG_ARGS+=(--ft --cmc --stop-time 8s --trace stdout)
    ;;
  cmc)
    FIG_ARGS+=(--ft --cmc --stop-time "$STOP_TIME")
    ;;
  isplit)
	# isplit 模式需要先从编译后的 IOSA 注释尾部提取 IFUN 表达式。
    IFUN_SPEC=""
    SPLIT_SPEC=""
    case "$IFUN_KIND" in
      STR)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN STRUCTURAL:/ { print $4 }' | tail -n 1)"
        ;;
      MCS)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN MCS:/ { print $4 }' | tail -n 1)"
        ;;
      TIME)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN TIME:/ { print $4 }' | tail -n 1)"
        ;;
      PHYS)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN PHYS:/ { print $4 }' | tail -n 1)"
        ;;
      TIMEPHYS)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN TIMEPHYS:/ { print $4 }' | tail -n 1)"
        ;;
      PWSTR)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN PWSTR:/ { print $4 }' | tail -n 1)"
        ;;
      T3)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN T3:/ { print $4 }' | tail -n 1)"
        ;;
      M3P)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN M3P:/ { print $4 }' | tail -n 1)"
        ;;
      N4)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N4:/ { print $4 }' | tail -n 1)"
        ;;
      N4P)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N4P:/ { print $4 }' | tail -n 1)"
        ;;
      N4Q)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N4Q:/ { print $4 }' | tail -n 1)"
        ;;
      N4A)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N4A:/ { print $4 }' | tail -n 1)"
        ;;
      N4AH)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N4AH:/ { print $4 }' | tail -n 1)"
        ;;
      N4AN)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N4AN:/ { print $4 }' | tail -n 1)"
        ;;
      ARDIV2)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN ARDIV2:/ { print $4 }' | tail -n 1)"
        ;;
      N5C)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N5C:/ { print $4 }' | tail -n 1)"
        ;;
      N5P)
        IFUN_SPEC="$(tail -n 40 "$IOSA_FILE" | awk '/IFUN N5P:/ { print $4 }' | tail -n 1)"
        ;;
      *)
        echo "ERROR: MODE=isplit requires IFUN_KIND in {STR,MCS,TIME,PHYS,TIMEPHYS,PWSTR,T3,M3P,N4,N4P,N4Q,N4A,N4AH,N4AN,ARDIV2,N5C,N5P}" >&2
        exit 3
        ;;
    esac

    if [ -z "$IFUN_SPEC" ]; then
      echo "ERROR: failed to extract IFUN from IOSA file: $IOSA_FILE" >&2
      exit 3
    fi

    if [ -n "$SPLIT_M_MAX" ] && [ "$SPLIT_M_MAX" -gt 1 ] 2>/dev/null && [ "$ENGINE" = "restart" ]; then
      if [ "$IFUN_KIND" = "TIMEPHYS" ]; then
        SPLIT_SPEC="$(tail -n 40 "$IOSA_FILE" | sed -n 's#^// SPLIT TIMEPHYS: ##p' | tail -n 1)"
      elif [ "$IFUN_KIND" = "PHYS" ]; then
        SPLIT_SPEC="$(tail -n 40 "$IOSA_FILE" | sed -n 's#^// SPLIT PHYS: ##p' | tail -n 1)"
        if [ -z "$SPLIT_SPEC" ]; then
          SPLIT_IOSA_FILE="$OUT_DIR/model_split_timephys.sa"
          SPLIT_KOMP_ARGS=("$KOMPILATOR" "$FT_FILE" "$SPLIT_IOSA_FILE" -f -t "$TRANSIENT_HORIZON" -i TIMEPHYS --phys-k "$PHYS_K" --phys-beta "$PHYS_BETA" --phys-gamma "$PHYS_GAMMA" --pand-eta "$PAND_ETA" --split-mmax "$SPLIT_M_MAX")
          if command -v conda >/dev/null 2>&1; then
            conda run -n 3.10 python "${SPLIT_KOMP_ARGS[@]}"
          else
            python3 "${SPLIT_KOMP_ARGS[@]}"
          fi
          SPLIT_SPEC="$(tail -n 40 "$SPLIT_IOSA_FILE" | sed -n 's#^// SPLIT TIMEPHYS: ##p' | tail -n 1)"
        fi
      fi
      if [ -z "$SPLIT_SPEC" ]; then
        echo "ERROR: failed to extract split formula from IOSA file: $IOSA_FILE" >&2
        exit 3
      fi
    fi

    if [ -z "$SPLIT_SPEC" ] && [ "$FORCE_SPLIT" = "1" ] && [ -n "$SPLIT_M_MAX" ] && [ "$SPLIT_M_MAX" -gt 1 ] 2>/dev/null; then
      if [ "$IFUN_KIND" = "TIMEPHYS" ]; then
        SPLIT_SPEC="$(tail -n 40 "$IOSA_FILE" | sed -n 's#^// SPLIT TIMEPHYS: ##p' | tail -n 1)"
      elif [ "$IFUN_KIND" = "PHYS" ]; then
        SPLIT_SPEC="$(tail -n 40 "$IOSA_FILE" | sed -n 's#^// SPLIT PHYS: ##p' | tail -n 1)"
        if [ -z "$SPLIT_SPEC" ]; then
          SPLIT_IOSA_FILE="$OUT_DIR/model_split_timephys.sa"
          SPLIT_KOMP_ARGS=("$KOMPILATOR" "$FT_FILE" "$SPLIT_IOSA_FILE" -f -t "$TRANSIENT_HORIZON" -i TIMEPHYS --phys-k "$PHYS_K" --phys-beta "$PHYS_BETA" --phys-gamma "$PHYS_GAMMA" --pand-eta "$PAND_ETA" --split-mmax "$SPLIT_M_MAX")
          if command -v conda >/dev/null 2>&1; then
            conda run -n 3.10 python "${SPLIT_KOMP_ARGS[@]}"
          else
            python3 "${SPLIT_KOMP_ARGS[@]}"
          fi
          SPLIT_SPEC="$(tail -n 40 "$SPLIT_IOSA_FILE" | sed -n 's#^// SPLIT TIMEPHYS: ##p' | tail -n 1)"
        fi
      fi
      if [ -z "$SPLIT_SPEC" ]; then
        echo "ERROR: forced split requested but no split formula could be extracted from IOSA file: $IOSA_FILE" >&2
        exit 3
      fi
    fi

    # Kepler emits IFUN specs as 'expr;min;max[;neutral]'.
    # Pass through the kompilator-computed bounds so FIG can trust them
    # (code path: minVal < maxVal → skip expensive state-space scan).
    # Fall back to 0;0 only when the kompilator didn't emit a positive max.
    IFS=';' read -r IFUN_EXPR _IFUN_MIN _IFUN_MAX IFUN_NEUT <<< "$IFUN_SPEC"
    if [ -n "${_IFUN_MAX:-}" ] && [ "${_IFUN_MAX:-0}" != "0" ]; then
      IFUN_SPEC_CONV="${IFUN_EXPR};${_IFUN_MIN:-0};${_IFUN_MAX}"
    else
      IFUN_SPEC_CONV="${IFUN_EXPR};0;0"
    fi
    if [ -n "${IFUN_NEUT:-}" ]; then
      IFUN_SPEC_CONV="${IFUN_SPEC_CONV};${IFUN_NEUT}"
    fi

    IFUN_ARG="$IFUN_SPEC_CONV"
    IFUN_FILE="$OUT_DIR/ifun.arg"
    printf '%s' "$IFUN_SPEC_CONV" > "$IFUN_FILE"
    IFUN_ARG="@$IFUN_FILE"

    # In FIG fault-tree mode (--ft), importance splitting requires a compositional
    # function (--acomp) over *module-local importance variables only*.
    # If the IFUN expression references IOSA state variables (e.g. *_enabled) or
    # other non-module symbols, --acomp will fail. Detect and fall back to an
    # algebraic importance function (--adhoc) without --ft when needed.
    if [[ "$IFUN_KIND" == "TIME" || "$IFUN_KIND" == "PHYS" || "$IFUN_KIND" == "TIMEPHYS" || "$IFUN_KIND" == "PWSTR" || "$IFUN_KIND" == "T3" || "$IFUN_KIND" == "M3P" || "$IFUN_KIND" == "N4" || "$IFUN_KIND" == "N4P" || "$IFUN_KIND" == "N4Q" || "$IFUN_KIND" == "N4A" || "$IFUN_KIND" == "N4AH" || "$IFUN_KIND" == "N4AN" || "$IFUN_KIND" == "ARDIV2" || "$IFUN_KIND" == "N5C" || "$IFUN_KIND" == "N5P" ]]; then
      USE_FT_ISPLIT="0"
    fi
    if [ "$USE_FT_ISPLIT" = "1" ] && ([[ "$IFUN_SPEC_CONV" == *"_enabled"* ]] || [[ "$IFUN_SPEC_CONV" == *"_phase"* ]]); then
      echo "WARN: IFUN references runtime state vars (*_enabled or *_phase); switching to USE_FT_ISPLIT=0 (no --ft, use --adhoc)" >&2
      USE_FT_ISPLIT="0"
    fi

    # Precomputed thresholds shortcut: bypass SMC/hyb rebuild when caller
    # provides THRESHOLDS_ADHOC (e.g. extracted from a previous run).
    THR_ARGS=()
    if [ -n "$THRESHOLDS_ADHOC" ]; then
      # Persist for debugging / reproducibility even though FIG does not
      # currently accept an @file for --thresholds-adhoc.
      printf '%s' "$THRESHOLDS_ADHOC" > "$OUT_DIR/thresholds.adhoc"
      THR_ARGS+=(--thresholds-adhoc "$THRESHOLDS_ADHOC")
    else
      THR_ARGS+=(-t "$THR_TECH")
    fi

    if [ "$USE_FT_ISPLIT" = "1" ]; then
	  # 纯 compositional IFUN 走 --ft + --acomp。
      # For fault-tree models ("--ft"), FIG requires the compositional importance
      # function ("--acomp"), using the IFUN as the composition expression.
      # NOTE: This typically only works when the IFUN expression depends solely
      # on module-local importance variables (no direct state var references).
      FIG_ARGS+=(--ft -e "$ENGINE" "${THR_ARGS[@]}" -g "$GLOBAL_EFFORT" --stop-time "$STOP_TIME" --acomp "$IFUN_ARG")
    else
	  # 否则回退到 --adhoc，允许 IFUN 直接引用状态变量或 PAND 辅助变量。
      # Default for this multi-phase+active model:
      # Use an algebraic ad-hoc importance function without "--ft".
      # This supports IFUN expressions that reference state variables like
      # '<be>_enabled' and PAND helper variables.
      FIG_ARGS+=(-e "$ENGINE" "${THR_ARGS[@]}" -g "$GLOBAL_EFFORT" --stop-time "$STOP_TIME" --adhoc "$IFUN_ARG")
    fi
    if [ -n "$SPLIT_SPEC" ]; then
      SPLIT_FILE="$OUT_DIR/split.arg"
      printf '%s' "$SPLIT_SPEC" > "$SPLIT_FILE"
      FIG_ARGS+=(--split "@$SPLIT_FILE")
    fi
    ;;
  *)
    echo "ERROR: Unknown MODE=$MODE (expected trace|cmc|isplit)" >&2
    exit 3
    ;;
esac

if [ "${N4Q_ORACLE_DIAG:-0}" = "1" ] || [ "${CLF_ATLAS_DIAG:-0}" = "1" ]; then
  FIG_ARGS+=(--batch-size 1)
fi

echo "STEP 3/3: run FIG"

set +e
TIME_BIN=""
if command -v /usr/bin/time >/dev/null 2>&1; then
  TIME_BIN="/usr/bin/time"
elif command -v time >/dev/null 2>&1; then
  TIME_BIN="time"
fi
TIME_FILE="$OUT_DIR/fig.time"
if [ "$FIG_TIMEOUT" -gt 0 ] 2>/dev/null; then
  if [ -n "$TIME_BIN" ]; then
    timeout "$FIG_TIMEOUT" "$TIME_BIN" -v -o "$TIME_FILE" "${FIG_ARGS[@]}" "$IOSA_FILE" "$PROP_FILE" > "$OUT_FILE" 2> "$ERR_FILE"
  else
    timeout "$FIG_TIMEOUT" "${FIG_ARGS[@]}" "$IOSA_FILE" "$PROP_FILE" > "$OUT_FILE" 2> "$ERR_FILE"
  fi
else
  if [ -n "$TIME_BIN" ]; then
    "$TIME_BIN" -v -o "$TIME_FILE" "${FIG_ARGS[@]}" "$IOSA_FILE" "$PROP_FILE" > "$OUT_FILE" 2> "$ERR_FILE"
  else
    "${FIG_ARGS[@]}" "$IOSA_FILE" "$PROP_FILE" > "$OUT_FILE" 2> "$ERR_FILE"
  fi
fi
EC=$?
set -e

if [ -s "$TIME_FILE" ]; then
  printf '\n' >> "$ERR_FILE"
  cat "$TIME_FILE" >> "$ERR_FILE"
fi

if [ "${SFE_COUNT_ONLY:-0}" != "0" ] && [ -s "$ERR_FILE" ]; then
  python3 - "$ERR_FILE" <<'PY'
import os
import re
import sys

path = sys.argv[1]
tmp = path + ".compact"
keys = (
    "production_trajectory_count",
    "simulation_step_count",
    "event_transition_count",
    "threshold_build_trajectory_count",
    "threshold_build_simulation_step_count",
    "threshold_build_event_transition_count",
    "threshold_build_wall_time_ms",
    "level_wall_time_ms",
)
sums = {key: 0.0 for key in keys}
summary_rows = 0

def parse_number(row, key):
    try:
        return float(row.get(key, "0") or "0")
    except ValueError:
        return 0.0

with open(path, "r", encoding="utf-8", errors="ignore") as src, \
     open(tmp, "w", encoding="utf-8") as dst:
    for line in src:
        if "SFE_COUNT_SUMMARY" not in line:
            dst.write(line)
            continue
        payload = line.split("SFE_COUNT_SUMMARY", 1)[1]
        row = dict(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)", payload))
        if not row:
            continue
        summary_rows += 1
        for key in keys:
            sums[key] += parse_number(row, key)
    if summary_rows:
        parts = [
            "SFE_COUNT_SUMMARY",
            "mode=aggregate",
            "summary_rows=%d" % summary_rows,
        ]
        for key in keys:
            value = sums[key]
            if key.endswith("_wall_time_ms"):
                parts.append("%s=%.17g" % (key, value))
            else:
                parts.append("%s=%d" % (key, int(round(value))))
        dst.write(" ".join(parts) + "\n")
os.replace(tmp, path)
PY
fi

if [ $EC -eq 124 ]; then
  echo "TIMEOUT: FIG killed after ${FIG_TIMEOUT}s wall-clock. See: $ERR_FILE" >&2
  exit $EC
fi

if [ $EC -ne 0 ]; then
  echo "ERROR: FIG failed (exit=$EC). See: $ERR_FILE" >&2
  exit $EC
fi

echo "OK"
echo "  OUT_DIR   : $OUT_DIR"
echo "  IOSA_FILE : $IOSA_FILE"
echo "  PROP_FILE : $PROP_FILE"
echo "  FIG_STDOUT: $OUT_FILE"
echo "  FIG_STDERR: $ERR_FILE"

if [ "$MODE" = "trace" ]; then
  echo "  Trace check:"
  grep -nE 'Action: PH_[0-9]+!!|PHASE_phase=' "$OUT_FILE" | head -n 50 || true
fi
