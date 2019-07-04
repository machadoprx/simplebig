#include "bn.h"

big_t*
big_new()
{
	big_t *t = malloc(sizeof(struct _big_t));
	memset(t->value, 0, BIG_WORDS_SIZE);
	t->sign = false;
	return t;
}

void
big_free(big_t *a)
{
	if (a == NULL) {
		return;
	}
	free(a);
}

int
big_get_lnt(big_t *a)
{
	register int k = BIG_MAX_DIGITS - 1;
	register dig_t *ap = a->value + k;

	while (*ap == 0ull && --k > 0) {
		ap--;
	}

	return k;
}

dig_t*
big_to_bin(big_t *a, int *lenght)
{
	dig_t *bin = malloc(512 * sizeof(dig_t));
	register dig_t *p = bin;
	register int k = big_get_lnt(a);	
	register dig_t *ap = a->value + k, t = a->value[k], mask = 0x80000000ull;
	memset(bin, 0, 512 * sizeof(dig_t));
	int mswbits = 0;

	while (t > 0) {
		t = t >> 1;
		mswbits++;
	}

	while (!(mask & (*ap))) {
		mask = mask >> 1;
	}

	for (; ap >= a->value; ap--) {
		while (mask != 0ull) {
			*p++ = (mask & (*ap));
			mask = mask >> 1;
		}
		mask = 0x80000000ull;
	}

	*lenght = mswbits + (k * BIG_DIGIT_BITS);

	return bin;
}

void
big_null(big_t *a)
{
	memset(a->value, 0, BIG_WORDS_SIZE);
	a->sign = false;
}

void
big_cpy(big_t *a, big_t *r)
{
	memcpy(r->value, a->value, BIG_WORDS_SIZE);
	r->sign = a->sign; 
}

dig_t
bin_to_int(char *num)
{
	register dig_t r = 0;
	register char *p = num, *stop = num + BIG_DIGIT_BITS - 1;

	while (p <= stop) {
		r = (*p++ == '1') ? (r << 1) ^ 1 : r << 1;
	}

	return r;
}

int 
big_gth(big_t *a, big_t *b)
{
	if (a->sign == b->sign) {

		return a->sign ? big_gth_uns(b, a) : big_gth_uns(a, b);

	}
	return a->sign ? 0 : 2;
}

int 
big_gth_uns(big_t *a, big_t *b)
{
	register int n = big_get_lnt(a);
	register int m = big_get_lnt(b);
	
	if (n != m) {
		return n > m ? 2 : 0;
	}

	register dig_t *ap = a->value + n;
    register dig_t *bp = b->value + n;
	register int i = n;

	while (i > 0 && *ap == *bp) {
		ap--; bp--; i--;
	}

	return (*ap < *bp) ? 0 : (*ap == *bp) ? 1 : 2;
}

static void
padd_str(char *str, char *dest, int padd)
{
	register char *p = str + (strlen(str) - 1);
	register char *m = dest + padd - 1;
	memset(dest, '0', sizeof(char) * padd);

	while (*p) {
		*m-- = *p--;
	}
}

void
bin_to_big(char *src, big_t *r)
{
	char num[512];
	padd_str(src, num, 512);
	char digit[BIG_DIGIT_BITS];
	register char *nump = num;
	register dig_t *rp = r->value + (BIG_MAX_DIGITS - 1);
	register char *digp, *stop = digit + BIG_DIGIT_BITS - 1;
	big_null(r);

	for (; rp >= r->value; rp--) {
		digp = digit;

		while (digp <= stop) {
			*(digp++) = *(nump++);
		}

		*rp = bin_to_int(digit);
	}
}

void
hex_to_big(char *src, big_t *r)
{
	char num[128];
	padd_str(src, num, 128);
	char digit[9];
	register char *nump = num;
	register dig_t *rp = r->value + (BIG_MAX_DIGITS - 1);
	register char *digp, *stop = digit + 7;
	big_null(r);

	for (; rp >= r->value; rp--) {
		digp = digit;

		while (digp <= stop) {
			*(digp++) = *(nump++);
		}
		*(digp) = '\0';

		*rp = strtoull(digit, NULL, 16);
	}
}

int
big_legendre_symbol(big_t *a, big_t *b, big_t *A, big_t *R, big_t *beta, big_t *Rm, big_t *pn)
{
	big_t t, r;
	big_rst(b, &t);
	big_mont_pow(a, &t, b, A, R, beta, Rm, pn, &r);
	return (r.value[0] > 1ull) ? -1 : r.value[0];
}

void
big_mont(big_t *a, big_t *b, big_t *Rm, big_t *beta, big_t *r)
{
	big_t t1;
	big_null(r);
	big_and(a, Rm, &t1);
	big_mul(&t1, beta, r);
	big_and(r, Rm, &t1);
	big_mul(&t1, b, r);
	big_sum(a, r, &t1);
	big_rst_word(&t1, 8, r);

	if (big_gth_uns(r, b) >= BIG_EQUAL) {
		big_sub(r, b, &t1);
		big_cpy(&t1, r);
	}
}

void
big_mont_pow(big_t *a, big_t *b, big_t *p, big_t *A, big_t *R, big_t *beta, big_t *Rm, big_t *pn, big_t *r)
{
	big_t xn;
	big_t t;
	int b_lenght;
	dig_t *bin_b = big_to_bin(b, &b_lenght);
	register dig_t *bit = bin_b;
	big_mul(a, R, &t);
	big_fastmod_25519(&t, p, pn, &xn);

	while (b_lenght--) {
		big_mul(A, A, &t);
		big_mont(&t, p, Rm, beta, A);
		if (*bit++) {
			big_mul(A, &xn, &t);
			big_mont(&t, p, Rm, beta, A);
		}
	}

	big_mont(A, p, Rm, beta, r);
	free(bin_b);
}

void
big_sum(big_t *a, big_t *b, big_t *r)
{
	if (a->sign != b->sign) {
		big_t a1;
		big_t b1;
		big_cpy(a, &a1);
		big_cpy(b, &b1);
		a1.sign = false;
		b1.sign = false;
		a->sign ? big_sub(&b1, &a1, r) : big_sub(&a1, &b1, r);
		return;
	}

	big_null(r);
	int n = big_get_lnt(a), m = big_get_lnt(b);
	register dig_t w, carry = 0;
	register dig_t *gp, *lp, *rp = r->value;

	if (n >= m) {
		lp = b->value;
		gp = a->value;
	}
	else {
		lp = a->value;
		gp = b->value;
		int t = n; n = m; m = t;
	}

	register dig_t *stop1 = r->value + n + 1, *stop2 = r->value + m;

	for (; rp <= stop2; lp++, gp++, rp++) {
		w = carry + (*gp) + (*lp);
		(*rp) = w & BIG_BASE_M;
		carry = w >> BIG_DIGIT_BITS;
	}

	while (carry) { // may overflow
		w = carry + (*gp++);
		(*rp++) = w & BIG_BASE_M;
		carry = w >> BIG_DIGIT_BITS;
	}

	while (rp <= stop1) {
		(*rp++) = (*gp++);
	}

	r->sign = a->sign ? true : false;
}

void
big_sub(big_t *a, big_t *b, big_t *r)
{
	if (a->sign != b->sign) {
		big_t a1;
		big_t b1;
		big_cpy(a, &a1);
		big_cpy(b, &b1);
		a1.sign = b1.sign = false;
		big_sum(&a1, &b1, r);
		r->sign = a->sign ? true : false;
		return;
	}

	if (a->sign == true) {
		big_t a1;
		big_t b1;
		big_cpy(a, &a1);
		big_cpy(b, &b1);
		a1.sign = b1.sign = false;
		big_sub(&b1, &a1, r);
		return;
	}

	int n = big_get_lnt(a);
	int m = big_get_lnt(b), t = -1;
	register int64_t borrow = 0;
	register dig_t *gp, *lp, *rp = r->value;
	big_null(r);

	if (big_gth_uns(a, b) > BIG_LESS) {
		gp = a->value;
		lp = b->value;
	}
	else {
		gp = b->value;
		lp = a->value;
		t = n; n = m; m = t; m++;
	}

	register dig_t *stop1 = r->value + n, *stop2 = r->value + m, shift = (8 * sizeof(dig_t) - 1);

	for (; rp <= stop2; gp++, lp++, rp++) {

		(*rp) = (*gp) - (*lp) - borrow;	// warning: mix int with uint
		borrow = (*rp) >> shift; // get msb (two complement's sign)
		(*rp) = (*rp) & BIG_BASE_M; // assign modulo to result
	}

	while (borrow) { // may overflow

		(*rp) = (*gp) - borrow;	// warning: mix int with uint
		borrow = (*rp) >> shift;
		(*rp) = (*rp) & BIG_BASE_M;

		rp++; gp++;
	}

	while (rp <= stop1) {
		(*rp++) = (*gp++);
	}

	r->sign = t == -1 ? false : true;
}

void
big_mul_nasty(big_t *a, big_t *b, big_t *r)
{
	big_null(r);
	int n = big_get_lnt(a);
	int m = big_get_lnt(b) + 1;
	register dig_t uv, u;
	register dig_t *ap = a->value, *bp, *rp = r->value, *trp, *stop1 = a->value + n, *stop2 = b->value + m;

	for (; ap <= stop1; ap++, rp++) {
		u = 0;
		for (trp = rp, bp = b->value; bp <= stop2; bp++, trp++) {
			uv = (*trp) + ((*ap) * (*bp)) + u;
			u = uv >> BIG_DIGIT_BITS;
			(*trp) = uv & BIG_BASE_M;
		} 
	}

	r->sign = (a->sign ^ b->sign);
}

void
big_sqr(big_t *a, big_t *r)
{
	big_null(r);
	int n = big_get_lnt(a);
	register twodig_t uv;
	register dig_t *tmpt, *ap = a->value, *rp = r->value, *tmpap, tmpx, u, *stop = (a->value + n);

	for (; ap <= stop; ap++, rp += 2) {
		
		uv = (*rp) + ((*ap) * (*ap));
		(*rp) = uv & BIG_BASE_M;
		u = uv >> BIG_DIGIT_BITS;
		tmpx = (*ap);
		tmpap = ap + 1;
		tmpt = rp + 1;

		for (; tmpap <= stop; tmpap++, tmpt++) {
			uv = tmpx * (*tmpap);
			uv = (*tmpt) + (uv << 1ull) + u;
			*tmpt = uv & BIG_BASE_M;
			u = uv >> BIG_DIGIT_BITS;
		} 

		while (u != 0ull) {
			uv = (*tmpt) + u;
			*(tmpt++) = uv & BIG_BASE_M;
			u = uv >> BIG_DIGIT_BITS;
		}
	}

	r->sign = false;
}

bool 
big_eql(big_t *a, big_t *b)
{
	if (a->sign != b->sign) {
		return false;
	}

	int n = big_get_lnt(a);
	int m = big_get_lnt(b);
	int g = n >= m ? n : m;
	register dig_t *ap = a->value, *bp = b->value, *stop = a->value + g;

	for (; ap <= stop; ap++, bp++) {
		
		if (*ap != *bp) {
			return false;
		}
	}

	return true;
}

void
big_and(big_t *a, big_t *b, big_t *r)
{
	int n = big_get_lnt(a);
	int m = big_get_lnt(b);
	int g = n >= m ? m : n;
	register dig_t *ap = a->value, *bp = b->value, *rp = r->value, *stop = r->value + g;
	big_null(r);

	for (; rp <= stop; rp++, ap++, bp++) {
		*rp = (*ap) & (*bp);
	}

	if (a->sign == true) {
		
		big_t one, t; 
		big_sub(b, r, &t);

		if ((*t.value) < BIG_BASE_M) {
			big_cpy(&t, r);
			(*r->value)++;
		}
		else {
			big_null(&one);
			(*one.value) = 1;
			big_sum(&t, &one, r);
		}
	}
}

void
big_rst_word(big_t *a, int s, big_t *r)
{
	int n = big_get_lnt(a) - s;
	register dig_t *ap = a->value + s, *rp = r->value, *stop = r->value + n;
	big_null(r);
	
	for (; rp <= stop; rp++, ap++) {
		*rp = *ap;
	}

	r->sign = a->sign;
}

void
big_rst(big_t *a, big_t *r)
{
	int n = big_get_lnt(a);
	register dig_t *ap = a->value + n, *rp = r->value + n, lsb = 0;
	big_null(r);
	
	for (; rp >= r->value; ap--, rp--) {
		*rp = lsb ? ((*ap) | BIG_BASE) >> 1ull : (*ap) >> 1ull;
		lsb = (*ap) & 1ull;
	}

	r->sign = a->sign;
}

void
big_lst_word(big_t *a, int s, big_t *r)
{
	int n = big_get_lnt(a) + s;
	register dig_t *ap = a->value, *rp = r->value + s, *stop = r->value + n;
	big_null(r);
	
	for (; rp <= stop; rp++, ap++) {
		*rp = *ap;
	}

	r->sign = a->sign;
}

void
big_lst(big_t *a, big_t *r)
{
	int n = big_get_lnt(a) + 1;
	register dig_t t, carry = 0;
	register dig_t *ap = a->value, *rp = r->value, *stop = r->value + n;
	big_null(r);

	for (; rp <= stop; rp++, ap++) {
		t = (*ap << 1ull) + carry;
		*rp = t & BIG_BASE_M;
		carry = t >> BIG_DIGIT_BITS;
	}

	r->sign = a->sign;
}

void
big_to_hex(big_t *a)
{
	int n = big_get_lnt(a);
	register dig_t *ap = a->value + n;

	fputs("0x", stdout);
	for (; ap >= a->value; --ap) {
		*ap > 0xFFFFFFFull ? printf("%lx", *ap) : printf("%08lx", *ap);
	}
	fputs("\n", stdout);
}

void
big_mod(big_t *a, big_t *p, big_t *r)
{
	big_t t;
	big_null(r);
	big_cpy(a, r);

	if (r->sign == false){
		while (big_gth_uns(r, p) > 0) {
			big_sub(r, p, &t);
			big_cpy(&t, r);
		}
	}

	else {
		while (r->sign == true) {
			big_sum(r, p, &t);
			big_cpy(&t, r);
		}
	}
}

void
big_mul(big_t *a, big_t *b, big_t *r)
{
	big_null(r);
	register int n = big_get_lnt(a);
	register int m = big_get_lnt(b);
	register int g = n + m + 1, i, j;
	register dig_t uv = 0, u = 0, *rp = r->value, *ap, *bp, *tbp;
	
	if (n < m) {
		int t = n;
		n = m; m = t;
	}

	bp = b->value;
	for (i = 0; i <= n; i++, rp++) {
		
		ap = a->value;
		bp = b->value + i;

		for (j = 0; j <= i; ap++ ,bp--, j++) {
			uv = uv + ((*ap) * (*bp));
			u = (uv >> BIG_DIGIT_BITS) + u;
			uv = uv & BIG_BASE_M;
		}

		(*rp) = uv & BIG_BASE_M;
		uv = u & BIG_BASE_M;
		u = u >> BIG_DIGIT_BITS;
	}

	bp = b->value + n;

	for (; i <= g; i++, rp++) {
		
		ap = a->value + (i - n);

		for (tbp = bp, j = i - n; j <= n; ap++ ,tbp--, j++) {
			uv = uv + ((*ap) * (*tbp));
			u = (uv >> BIG_DIGIT_BITS) + u;
			uv = uv & BIG_BASE_M;
		}
		
		(*rp) = uv & BIG_BASE_M;
		uv = u & BIG_BASE_M;
		u = u >> BIG_DIGIT_BITS;
	}

	r->sign = a->sign ^ b->sign;
}

void
big_fastmod_25519(big_t *a, big_t *p, big_t *pn, big_t *r)
{
	big_t t1, t2, t3, tmpq, tmpk;
	dig_t msb, *r7 = r->value + 7;
	big_null(&tmpk);
	tmpk.value[0] = 19ull;
	big_cpy(a, r);
	r->sign = false;

	while (big_gth_uns(r, p) > BIG_LESS) {

		msb = (*r7) & 0x80000000ull; // b
		big_rst_word(r, 8, &t1);
		big_lst(&t1, &tmpq);

		msb = msb >> 31;
		(*tmpq.value) = (*tmpq.value) + msb;

		big_and(r, pn, &t1);
		big_mul(&tmpq, &tmpk, &t2);
		big_sum(&t1, &t2, r);
		
		if (big_gth_uns(r, p) > BIG_LESS) {
			
			big_sub(r, p, &t3);
			big_cpy(&t3, r);
		}
	}

	if (a->sign == true) {
		big_cpy(r, &t1);
		big_sub(p, &t1, r);
	}
}

void
big_mod_inv(big_t *a, big_t *b, big_t *r)
{
	big_t u, v, x1, x2, one, t; 
	big_null(r);
	big_cpy(a, &u);
	big_cpy(b, &v);
	big_null(&x1);
	big_null(&x2);
	big_null(&one);
	x1.value[0] = one.value[0] = 1ull;

	while (!big_eql(&u, &one) && !big_eql(&v, &one)) {

		while (!(*(u.value) & 1ull)) {
			big_rst(&u, &t);
			big_cpy(&t, &u);
			if ((*(x1.value) & 1ull)) {
				big_sum(&x1, b, &t);
				big_cpy(&t, &x1);
			}
			big_rst(&x1, &t);
			big_cpy(&t, &x1);
		}

		while (!(*(v.value) & 1ull)) {
			big_rst(&v, &t);
			big_cpy(&t, &v);
			if ((*(x2.value) & 1ull)) {
				big_sum(&x2, b, &t);
				big_cpy(&t, &x2);
			}
			big_rst(&x2, &t);
			big_cpy(&t, &x2);
		}

		if (big_gth(&u, &v) >= BIG_EQUAL) {
			big_sub(&u, &v, &t);
			big_cpy(&t, &u);
			big_sub(&x1, &x2, &t);
			big_cpy(&t, &x1);
		}

		else {
			big_sub(&v, &u, &t);
			big_cpy(&t, &v);
			big_sub(&x2, &x1, &t);
			big_cpy(&t, &x2);
		}
		
	}

	big_eql(&u, &one) ? big_mod(&x1, b, r) : big_mod(&x2, b, r);
}

void
big_rand_8dig(big_t *r)
{
	big_null(r);

	register dig_t *rp = r->value, *tmprp = r->value;
	unsigned long long rand_digit;
	__builtin_ia32_rdrand64_step(&rand_digit);
	*(rp++) = rand_digit & BIG_BASE_M;

	for (int i = 1; i < 8; i++, rp++, tmprp++) {
		__builtin_ia32_rdrand64_step(&rand_digit);
		(*rp) = (rand_digit ^ (*tmprp)) & BIG_BASE_M;
	}
	//rp--;
	//(*rp) = *rp & 0x1FFFFFFF;
}