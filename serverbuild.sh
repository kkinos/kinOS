
for MK in $(ls servers/*/Makefile)
do
  SERV_DIR=$(dirname $MK)
  SERV=$(basename $SERV_DIR)
  make ${MAKE_OPTS:-} -C $SERV_DIR $SERV
done