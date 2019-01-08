import os
import sys

# The first argument is the directory containing the file whose RPATH will be computed.
orig = sys.argv[1]

# The other parameters are directories whose relative position has to be added to RPATH.
for dep in sys.argv[2:3]:
    sys.stdout.write('$ORIGIN/' + os.path.relpath(dep,orig))
for dep in sys.argv[3:]:
    sys.stdout.write(':$ORIGIN/' + os.path.relpath(dep,orig))

sys.stdout.write('\n')

