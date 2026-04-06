#!/bin/bash
# ============================================================================
# Project 3 Demo Script
# CSCI 331 - Zip Code Blocked Sequence Set
# ============================================================================

BSS="zipcode.bss"
SIDX="zipcode.sidx"
SMALL_BSS="small.bss"
SMALL_SIDX="small.sidx"
CSV="us_postal_codes.csv"
SMALL_CSV="small.csv"
BLOCK=256   # small block size for demo (fits ~4-5 records per block)

echo "============================================================"
echo " Project 3 Demo — Blocked Sequence Set"
echo "============================================================"
echo ""

# ── 1. Create full blocked sequence set (default 512-byte blocks) ────────────
echo "--- [1] Create full dataset (512-byte blocks) ---"
./zip3 --create "$CSV" "$BSS" "$SIDX"
echo ""

# ── 2. Create small dataset (256-byte blocks, ~4-5 records/block) ───────────
echo "--- [2] Create small dataset (${BLOCK}-byte blocks) ---"
./zip3 --create "$SMALL_CSV" "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 3. Print file header ────────────────────────────────────────────────────
echo "--- [3] File header ---"
./zip3 --header "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 4. Physical + Logical dump BEFORE modifications ─────────────────────────
echo "--- [4] Initial physical + logical dump ---"
./zip3 --dump "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 5. Index dump ────────────────────────────────────────────────────────────
echo "--- [5] Index dump ---"
./zip3 --dump-index "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 6. Search: valid and invalid ZIPs ────────────────────────────────────────
echo "--- [6] Search full dataset (valid + invalid ZIPs) ---"
./zip3 --search "$BSS" "$SIDX" \
    -Z56301 -Z99546 -Z90210 -Z01001 -Z02139 -Z00001 -Z99999
echo ""

# ── 7. Add records: no split + split required ────────────────────────────────
echo "--- [7] Add records (includes block split) ---"
./zip3 --add "$SMALL_BSS" "$SMALL_SIDX" small_add.csv $BLOCK
echo ""

echo "--- [7b] Dump AFTER adds ---"
./zip3 --dump "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""
./zip3 --dump-index "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 8. Delete: no merge ──────────────────────────────────────────────────────
echo "--- [8] Delete records (no merge required) ---"
cat > _del_simple.txt << EOF
01002
01005
EOF
./zip3 --delete "$SMALL_BSS" "$SMALL_SIDX" _del_simple.txt $BLOCK
echo ""
echo "--- [8b] Dump AFTER simple deletes ---"
./zip3 --dump "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 9. Delete: redistribution ────────────────────────────────────────────────
echo "--- [9] Delete records (triggers redistribution) ---"
cat > _del_redist.txt << EOF
01009
01013
EOF
./zip3 --delete "$SMALL_BSS" "$SMALL_SIDX" _del_redist.txt $BLOCK
echo ""
echo "--- [9b] Dump AFTER redistribution ---"
./zip3 --dump "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 10. Delete: merge + avail list ───────────────────────────────────────────
echo "--- [10] Delete records (triggers merge, block → avail list) ---"
cat > _del_merge.txt << EOF
01014
01020
EOF
./zip3 --delete "$SMALL_BSS" "$SMALL_SIDX" _del_merge.txt $BLOCK
echo ""
echo "--- [10b] Dump AFTER merge (avail list should be non-empty) ---"
./zip3 --dump "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""
./zip3 --dump-index "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 11. Insert after merge: avail block reused ───────────────────────────────
echo "--- [11] Re-insert to confirm avail block reused ---"
cat > _readd.csv << EOF
01009,Ware,MA,Hampshire,42.2601,-72.2445
01015,Chicopee,MA,Hampden,42.1512,-72.6044
01016,Chicopee,MA,Hampden,42.1600,-72.6100
01017,Chicopee,MA,Hampden,42.1700,-72.6200
01018,Chicopee,MA,Hampden,42.1800,-72.6300
EOF
./zip3 --add "$SMALL_BSS" "$SMALL_SIDX" _readd.csv $BLOCK
echo ""
echo "--- [11b] Dump — avail list should be empty again ---"
./zip3 --dump "$SMALL_BSS" "$SMALL_SIDX" $BLOCK
echo ""

# ── 12. Project-1 analysis via sequential scan ───────────────────────────────
echo "--- [12] Project-1 state extremes analysis (sequential scan) ---"
./zip3 --analyze "$BSS" "$SIDX"
echo ""

# ── Cleanup temp files ───────────────────────────────────────────────────────
rm -f _del_simple.txt _del_redist.txt _del_merge.txt _readd.csv

echo "============================================================"
echo " Demo complete."
echo "============================================================"
