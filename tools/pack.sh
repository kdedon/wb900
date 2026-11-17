#!/bin/sh
# pack.sh DIR NAME VERSION PATH=FILE|PATH/ ...: stage release package
# c900-NAME-vVERSION in DIR (PATH/ makes an empty directory), stamp it
# with VERSION, .provenance and .contents, and archive it.
set -e
dir=$1 name=$2 v=$3
shift 3
top=c900-$name-v$v
t=$dir/$top
rm -rf "$t" "$t.tar.gz"
mkdir -p "$t"
for a; do
	p=${a%%=*}
	case $a in
	*/) mkdir -p "$t/$a" ;;
	*) mkdir -p "$t/$(dirname "$p")"; cp "${a#*=}" "$t/$p" ;;
	esac
done
echo "$v" > "$t/VERSION"
root=$(cd "$(dirname "$0")/.." && pwd)
dirty=0
[ -z "$(git -C "$root" status --porcelain)" ] || dirty=1
cat > "$t/.provenance" <<END
kind=$name
package=$name
version=$v
commit=$(git -C "$root" rev-parse HEAD)
dirty=$dirty
built=$(date -u +%Y-%m-%dT%H:%M:%SZ)
END
# md5sum listing of the payload; contentid is its sha1, as the dist checks.
(cd "$t" && find . -type f ! -name .contents ! -name .provenance | LC_ALL=C sort |
	sed 's|^\./||' | xargs md5sum) > "$t/.contents"
echo "contentid=$(sha1sum "$t/.contents" | cut -c1-12)" >> "$t/.provenance"
tar -C "$dir" -czf "$t.tar.gz" "$top"
echo "$t.tar.gz"
