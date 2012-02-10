#!/bin/sh

set -e

echo -e '?hello a ab \\  \\ a a\\  \\\\\\  \n?echo abcdefghijklmnopqrstuvwxyz a'  | ./line | cat -A > test-line.output

cat << EOF > test-line.template
arg[0]: ?hello$
arg[1]: a$
arg[2]: ab$
arg[3]:  $
arg[4]:  a$
arg[5]: a $
arg[6]: \ $
arg[0]: ?echo$
arg[1]: abcdefghijklmnopqrstuvwxyz$
arg[2]: a$
EOF

diff -u test-line.template test-line.output

rm test-line.template test-line.output

