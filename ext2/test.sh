#!/usr/bin/env bash
set -euo pipefail

WORK=/work
TDIR=/home/testuser/t

die() { echo "FAIL: $*" >&2; exit 1; }

echo "==> Installing packages"
DEBIAN_FRONTEND=noninteractive apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    gcc e2fsprogs util-linux coreutils valgrind sudo >/dev/null

echo "==> Building"
gcc -Wall -Wextra -g -o "$WORK/ext2stat" "$WORK/ext2stat.c"
gcc -Wall -Wextra -g -o "$WORK/ext2cat"  "$WORK/ext2cat.c"
gcc -Wall -Wextra -g -o "$WORK/ext2ls"   "$WORK/ext2ls.c"
chmod 755 "$WORK"/ext2stat "$WORK"/ext2cat "$WORK"/ext2ls

echo "==> Setup: loop devices, testuser"
modprobe loop 2>/dev/null || true
for i in $(seq 0 7); do [ -b "/dev/loop$i" ] || mknod "/dev/loop$i" b 7 "$i"; done

id testuser &>/dev/null || useradd -m testuser
usermod -aG disk testuser 2>/dev/null || true
echo "testuser ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/testuser-ext2
chmod 440 /etc/sudoers.d/testuser-ext2
mkdir -p "$TDIR" && chown testuser:testuser "$TDIR"

export TDIR WORK
su -s /bin/bash testuser << 'EOF'
set -euo pipefail

IMG=$TDIR/ext2.img
MNT=$TDIR/mnt

PASS=0; FAIL=0
ok()  { echo "  PASS: $1"; PASS=$((PASS+1)); }
fail(){ echo "  FAIL: $1"; FAIL=$((FAIL+1)); }
chk() { local d=$1; shift; if "$@" 2>/dev/null; then ok "$d"; else fail "$d"; fi; }
chkeq() {
    local d=$1 got=$2 want=$3
    if [ "$got" = "$want" ]; then ok "$d"; else fail "$d (got=$got want=$want)"; fi
}

sha() { sha512sum "$@" | awk '{print $1}'; }

pipe_sha() {
    local dev=$1 ino=$2 bs=$3 skip=$4 cnt=$5
    (set +o pipefail
     "$WORK/ext2cat" "$dev" "$ino" \
       | dd iflag=fullblock bs="$bs" skip="$skip" count="$cnt" 2>/dev/null \
       | sha512sum | awk '{print $1}')
}

echo ""
echo "==> Creating filesystem"
truncate --size 512M "$IMG"
mkfs.ext2 -b 2048 -g 4096 -N 512 "$IMG" >/dev/null 2>&1
dumpe2fs -h "$IMG" 2>/dev/null | grep -q "0xEF53" || { echo "bad magic"; exit 1; }
mkdir -p "$MNT"
MLOOP=$(sudo losetup -f)
sudo losetup "$MLOOP" "$IMG"
sudo mount -t ext2 "$MLOOP" "$MNT"
sudo chmod 777 "$MNT"

echo "==> Populating"
dd if=/dev/urandom bs=8192  count=1  of="$MNT/small.bin"  2>/dev/null
dd if=/dev/urandom bs=1M    count=6  of="$MNT/medium.bin" 2>/dev/null
TBLK=262668  # first block of triple-indirect region: (12+512+512^2), bsz=2048
truncate --size 5G "$MNT/sparse.bin"
dd if=/dev/urandom bs=1M   count=1 of="$MNT/sparse.bin" conv=notrunc seek=0      2>/dev/null
dd if=/dev/urandom bs=2048 count=1 of="$MNT/sparse.bin" conv=notrunc seek=$TBLK  2>/dev/null
mkdir -p "$MNT/dir1" "$MNT/dir2/sub"
echo "file1" > "$MNT/dir1/a.txt"
echo "file2" > "$MNT/dir2/b.txt"

INO_SMALL=$(stat  -c%i "$MNT/small.bin")
INO_MEDIUM=$(stat -c%i "$MNT/medium.bin")
INO_SPARSE=$(stat -c%i "$MNT/sparse.bin")
INO_DIR1=$(stat   -c%i "$MNT/dir1")
INO_DIR2=$(stat   -c%i "$MNT/dir2")
INO_A=$(stat      -c%i "$MNT/dir1/a.txt")
INO_SUB=$(stat    -c%i "$MNT/dir2/sub")

SHA_SMALL=$(sha  "$MNT/small.bin")
SHA_MEDIUM=$(sha "$MNT/medium.bin")
SHA_A=$(sha      "$MNT/dir1/a.txt")
SHA_SP0=$(dd if="$MNT/sparse.bin" iflag=fullblock bs=1M      skip=0     count=1 2>/dev/null | sha512sum | awk '{print $1}')
SHA_SPT=$(dd if="$MNT/sparse.bin" iflag=fullblock bs=2048    skip=$TBLK count=1 2>/dev/null | sha512sum | awk '{print $1}')

sync
sudo umount "$MNT"
sudo losetup -d "$MLOOP"

echo ""
echo "==> Checksums (image)"
chkeq "small.bin"          "$(pipe_sha $IMG $INO_SMALL  1M    0     1)"   "$SHA_SMALL"
chkeq "medium.bin"         "$(pipe_sha $IMG $INO_MEDIUM 1M    0     6)"   "$SHA_MEDIUM"
chkeq "dir1/a.txt"         "$("$WORK/ext2cat" $IMG $INO_A | sha)"         "$SHA_A"
chkeq "sparse chunk@0"     "$(pipe_sha $IMG $INO_SPARSE 1M    0     1)"   "$SHA_SP0"
chkeq "sparse chunk@triple" "$(pipe_sha $IMG $INO_SPARSE 2048  $TBLK 1)"  "$SHA_SPT"

echo ""
echo "==> ext2stat"
chk "small:   regular file"    bash -c "$WORK/ext2stat $IMG $INO_SMALL  | grep -q 'regular file'"
chk "medium:  Double-indirect" bash -c "$WORK/ext2stat $IMG $INO_MEDIUM | grep -q 'Double-indirect'"
chk "sparse:  Triple-indirect" bash -c "$WORK/ext2stat $IMG $INO_SPARSE | grep -q 'Triple-indirect'"
chk "sparse:  size=5G"         bash -c "$WORK/ext2stat $IMG $INO_SPARSE | grep -q 'Size:.*5368709120'"
chk "dir1:    directory"       bash -c "$WORK/ext2stat $IMG $INO_DIR1   | grep -q 'directory'"

echo ""
echo "==> ext2ls"
LS1=$("$WORK/ext2cat" "$IMG" "$INO_DIR1" | "$WORK/ext2ls")
LS2=$("$WORK/ext2cat" "$IMG" "$INO_DIR2" | "$WORK/ext2ls")
chk "dir1 has a.txt"   bash -c "echo '$LS1' | grep -q 'a.txt'"
chk "dir1 inode a.txt" bash -c "echo '$LS1' | awk '/a.txt/{print \$1}' | grep -qx $INO_A"
chk "dir2 has sub"     bash -c "echo '$LS2' | grep -q 'sub'"
chk "dir2 inode sub"   bash -c "echo '$LS2' | awk '/sub/{print \$1}' | grep -qx $INO_SUB"

echo ""
echo "==> Loop device"
LOOP=$(sudo losetup -f)
sudo losetup "$LOOP" "$IMG"
sudo chmod a+r "$LOOP"
echo "  lsblk:"; lsblk -o name,size,fstype 2>/dev/null || true
chkeq "[loop] small.bin"  "$(pipe_sha $LOOP $INO_SMALL  1M 0 1)"  "$SHA_SMALL"
chkeq "[loop] medium.bin" "$(pipe_sha $LOOP $INO_MEDIUM 1M 0 6)"  "$SHA_MEDIUM"
LS1L=$("$WORK/ext2cat" "$LOOP" "$INO_DIR1" | "$WORK/ext2ls")
chk "[loop] dir1 has a.txt" bash -c "echo '$LS1L' | grep -q 'a.txt'"
sudo losetup -d "$LOOP"

echo ""
echo "==> Valgrind"
VG="valgrind --leak-check=full --error-exitcode=1 --quiet"
chk "vg ext2stat small"  bash -c "$VG $WORK/ext2stat $IMG $INO_SMALL  >/dev/null"
chk "vg ext2cat small"   bash -c "$VG $WORK/ext2cat  $IMG $INO_SMALL  >/dev/null"
chk "vg ext2cat|ext2ls"  bash -c "$VG $WORK/ext2cat  $IMG $INO_DIR1 | $VG $WORK/ext2ls >/dev/null"

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
EOF
