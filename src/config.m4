PHP_ARG_ENABLE(minienc, whether to enable minienc support,
[  --enable-minienc           Enable minienc support])

if test "$PHP_MINIENC" != "no"; then
PHP_ADD_LIBRARY(crypto, 1, MINIENC_SHARED_LIBADD)
PHP_SUBST(MINIENC_SHARED_LIBADD)
PHP_NEW_EXTENSION(minienc, minienc.c, $ext_shared)
fi