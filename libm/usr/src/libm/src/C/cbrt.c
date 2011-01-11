/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)cbrt.c	1.16	06/01/31 SMI"

/* INDENT OFF */

/*
 *  cbrt: double precision cube root
 *
 *  Algorithm: bit hacking, table lookup, and polynomial approximation
 *
 *  For normal x, write x = s*2^(3j)*z where s = +/-1, j is an integer,
 *  and 1 <= z < 8.  Let y := s*2^j.  From a table, find u such that
 *  u^3 is computable exactly and |(z-u^3)/u^3| <~ 2^-8.  We construct
 *  y, z, and the table index from x by a few integer operations.
 *
 *  Now cbrt(x) = y*u*(1+t)^(1/3) where t = (z-u^3)/u^3.  We approximate
 *  (1+t)^(1/3) by a polynomial 1+p(t), where p(t) := t*(p1+t*(p2+...+
 *  (p5+t*p6))).  By computing the result as y*(u+u*p(t)), we can bound
 *  the worst case error by .51 ulp.
 *
 *  Notes:
 *
 *  1. For subnormal x, we scale x by 2^54, compute the cube root, and
 *     scale the result by 2^-18.
 *
 *  2. cbrt(+/-inf) = +/-inf and cbrt(NaN) is NaN.
 */

/*
 * for i = 0, ..., 385
 *   form x(i) with high word 0x3ff00000 + (i << 13) and low word 0;
 *   then TBL[i] = cbrt(x(i)) rounded to 17 significant bits
 */
static const double __libm_TBL_cbrt[] = {
 1.00000000000000000e+00, 1.00259399414062500e+00, 1.00518798828125000e+00,
 1.00775146484375000e+00, 1.01031494140625000e+00, 1.01284790039062500e+00,
 1.01538085937500000e+00, 1.01791381835937500e+00, 1.02041625976562500e+00,
 1.02290344238281250e+00, 1.02539062500000000e+00, 1.02786254882812500e+00,
 1.03031921386718750e+00, 1.03277587890625000e+00, 1.03520202636718750e+00,
 1.03762817382812500e+00, 1.04003906250000000e+00, 1.04244995117187500e+00,
 1.04483032226562500e+00, 1.04721069335937500e+00, 1.04959106445312500e+00,
 1.05194091796875000e+00, 1.05429077148437500e+00, 1.05662536621093750e+00,
 1.05895996093750000e+00, 1.06127929687500000e+00, 1.06358337402343750e+00,
 1.06587219238281250e+00, 1.06816101074218750e+00, 1.07044982910156250e+00,
 1.07270812988281250e+00, 1.07496643066406250e+00, 1.07722473144531250e+00,
 1.07945251464843750e+00, 1.08168029785156250e+00, 1.08390808105468750e+00,
 1.08612060546875000e+00, 1.08831787109375000e+00, 1.09051513671875000e+00,
 1.09269714355468750e+00, 1.09487915039062500e+00, 1.09704589843750000e+00,
 1.09921264648437500e+00, 1.10136413574218750e+00, 1.10350036621093750e+00,
 1.10563659667968750e+00, 1.10775756835937500e+00, 1.10987854003906250e+00,
 1.11198425292968750e+00, 1.11408996582031250e+00, 1.11618041992187500e+00,
 1.11827087402343750e+00, 1.12034606933593750e+00, 1.12242126464843750e+00,
 1.12448120117187500e+00, 1.12654113769531250e+00, 1.12858581542968750e+00,
 1.13063049316406250e+00, 1.13265991210937500e+00, 1.13468933105468750e+00,
 1.13670349121093750e+00, 1.13871765136718750e+00, 1.14073181152343750e+00,
 1.14273071289062500e+00, 1.14471435546875000e+00, 1.14669799804687500e+00,
 1.14868164062500000e+00, 1.15065002441406250e+00, 1.15260314941406250e+00,
 1.15457153320312500e+00, 1.15650939941406250e+00, 1.15846252441406250e+00,
 1.16040039062500000e+00, 1.16232299804687500e+00, 1.16424560546875000e+00,
 1.16616821289062500e+00, 1.16807556152343750e+00, 1.16998291015625000e+00,
 1.17189025878906250e+00, 1.17378234863281250e+00, 1.17567443847656250e+00,
 1.17755126953125000e+00, 1.17942810058593750e+00, 1.18128967285156250e+00,
 1.18315124511718750e+00, 1.18501281738281250e+00, 1.18685913085937500e+00,
 1.18870544433593750e+00, 1.19055175781250000e+00, 1.19238281250000000e+00,
 1.19421386718750000e+00, 1.19602966308593750e+00, 1.19786071777343750e+00,
 1.19966125488281250e+00, 1.20147705078125000e+00, 1.20327758789062500e+00,
 1.20507812500000000e+00, 1.20686340332031250e+00, 1.20864868164062500e+00,
 1.21043395996093750e+00, 1.21220397949218750e+00, 1.21397399902343750e+00,
 1.21572875976562500e+00, 1.21749877929687500e+00, 1.21925354003906250e+00,
 1.22099304199218750e+00, 1.22274780273437500e+00, 1.22448730468750000e+00,
 1.22621154785156250e+00, 1.22795104980468750e+00, 1.22967529296875000e+00,
 1.23138427734375000e+00, 1.23310852050781250e+00, 1.23481750488281250e+00,
 1.23652648925781250e+00, 1.23822021484375000e+00, 1.23991394042968750e+00,
 1.24160766601562500e+00, 1.24330139160156250e+00, 1.24497985839843750e+00,
 1.24665832519531250e+00, 1.24833679199218750e+00, 1.25000000000000000e+00,
 1.25166320800781250e+00, 1.25332641601562500e+00, 1.25497436523437500e+00,
 1.25663757324218750e+00, 1.25828552246093750e+00, 1.25991821289062500e+00,
 1.26319885253906250e+00, 1.26644897460937500e+00, 1.26968383789062500e+00,
 1.27290344238281250e+00, 1.27612304687500000e+00, 1.27931213378906250e+00,
 1.28248596191406250e+00, 1.28564453125000000e+00, 1.28878784179687500e+00,
 1.29191589355468750e+00, 1.29502868652343750e+00, 1.29812622070312500e+00,
 1.30120849609375000e+00, 1.30427551269531250e+00, 1.30732727050781250e+00,
 1.31036376953125000e+00, 1.31340026855468750e+00, 1.31640625000000000e+00,
 1.31941223144531250e+00, 1.32238769531250000e+00, 1.32536315917968750e+00,
 1.32832336425781250e+00, 1.33126831054687500e+00, 1.33419799804687500e+00,
 1.33712768554687500e+00, 1.34002685546875000e+00, 1.34292602539062500e+00,
 1.34580993652343750e+00, 1.34867858886718750e+00, 1.35153198242187500e+00,
 1.35437011718750000e+00, 1.35720825195312500e+00, 1.36003112792968750e+00,
 1.36283874511718750e+00, 1.36564636230468750e+00, 1.36842346191406250e+00,
 1.37120056152343750e+00, 1.37396240234375000e+00, 1.37672424316406250e+00,
 1.37945556640625000e+00, 1.38218688964843750e+00, 1.38491821289062500e+00,
 1.38761901855468750e+00, 1.39031982421875000e+00, 1.39302062988281250e+00,
 1.39569091796875000e+00, 1.39836120605468750e+00, 1.40101623535156250e+00,
 1.40367126464843750e+00, 1.40631103515625000e+00, 1.40893554687500000e+00,
 1.41156005859375000e+00, 1.41416931152343750e+00, 1.41676330566406250e+00,
 1.41935729980468750e+00, 1.42193603515625000e+00, 1.42449951171875000e+00,
 1.42706298828125000e+00, 1.42962646484375000e+00, 1.43215942382812500e+00,
 1.43469238281250000e+00, 1.43722534179687500e+00, 1.43974304199218750e+00,
 1.44224548339843750e+00, 1.44474792480468750e+00, 1.44723510742187500e+00,
 1.44972229003906250e+00, 1.45219421386718750e+00, 1.45466613769531250e+00,
 1.45712280273437500e+00, 1.45956420898437500e+00, 1.46200561523437500e+00,
 1.46444702148437500e+00, 1.46687316894531250e+00, 1.46928405761718750e+00,
 1.47169494628906250e+00, 1.47409057617187500e+00, 1.47648620605468750e+00,
 1.47886657714843750e+00, 1.48124694824218750e+00, 1.48361206054687500e+00,
 1.48597717285156250e+00, 1.48834228515625000e+00, 1.49067687988281250e+00,
 1.49302673339843750e+00, 1.49536132812500000e+00, 1.49768066406250000e+00,
 1.50000000000000000e+00, 1.50230407714843750e+00, 1.50460815429687500e+00,
 1.50691223144531250e+00, 1.50920104980468750e+00, 1.51148986816406250e+00,
 1.51376342773437500e+00, 1.51603698730468750e+00, 1.51829528808593750e+00,
 1.52055358886718750e+00, 1.52279663085937500e+00, 1.52503967285156250e+00,
 1.52728271484375000e+00, 1.52951049804687500e+00, 1.53173828125000000e+00,
 1.53395080566406250e+00, 1.53616333007812500e+00, 1.53836059570312500e+00,
 1.54055786132812500e+00, 1.54275512695312500e+00, 1.54493713378906250e+00,
 1.54711914062500000e+00, 1.54928588867187500e+00, 1.55145263671875000e+00,
 1.55361938476562500e+00, 1.55577087402343750e+00, 1.55792236328125000e+00,
 1.56005859375000000e+00, 1.56219482421875000e+00, 1.56433105468750000e+00,
 1.56645202636718750e+00, 1.56857299804687500e+00, 1.57069396972656250e+00,
 1.57279968261718750e+00, 1.57490539550781250e+00, 1.57699584960937500e+00,
 1.57908630371093750e+00, 1.58117675781250000e+00, 1.58325195312500000e+00,
 1.58532714843750000e+00, 1.58740234375000000e+00, 1.59152221679687500e+00,
 1.59562683105468750e+00, 1.59970092773437500e+00, 1.60375976562500000e+00,
 1.60780334472656250e+00, 1.61183166503906250e+00, 1.61582946777343750e+00,
 1.61981201171875000e+00, 1.62376403808593750e+00, 1.62770080566406250e+00,
 1.63162231445312500e+00, 1.63552856445312500e+00, 1.63941955566406250e+00,
 1.64328002929687500e+00, 1.64714050292968750e+00, 1.65097045898437500e+00,
 1.65476989746093750e+00, 1.65856933593750000e+00, 1.66235351562500000e+00,
 1.66610717773437500e+00, 1.66986083984375000e+00, 1.67358398437500000e+00,
 1.67729187011718750e+00, 1.68098449707031250e+00, 1.68466186523437500e+00,
 1.68832397460937500e+00, 1.69197082519531250e+00, 1.69560241699218750e+00,
 1.69921875000000000e+00, 1.70281982421875000e+00, 1.70640563964843750e+00,
 1.70997619628906250e+00, 1.71353149414062500e+00, 1.71707153320312500e+00,
 1.72059631347656250e+00, 1.72410583496093750e+00, 1.72760009765625000e+00,
 1.73109436035156250e+00, 1.73455810546875000e+00, 1.73800659179687500e+00,
 1.74145507812500000e+00, 1.74488830566406250e+00, 1.74829101562500000e+00,
 1.75169372558593750e+00, 1.75508117675781250e+00, 1.75846862792968750e+00,
 1.76182556152343750e+00, 1.76516723632812500e+00, 1.76850891113281250e+00,
 1.77183532714843750e+00, 1.77514648437500000e+00, 1.77844238281250000e+00,
 1.78173828125000000e+00, 1.78500366210937500e+00, 1.78826904296875000e+00,
 1.79151916503906250e+00, 1.79476928710937500e+00, 1.79798889160156250e+00,
 1.80120849609375000e+00, 1.80441284179687500e+00, 1.80760192871093750e+00,
 1.81079101562500000e+00, 1.81396484375000000e+00, 1.81712341308593750e+00,
 1.82026672363281250e+00, 1.82341003417968750e+00, 1.82653808593750000e+00,
 1.82965087890625000e+00, 1.83276367187500000e+00, 1.83586120605468750e+00,
 1.83894348144531250e+00, 1.84201049804687500e+00, 1.84507751464843750e+00,
 1.84812927246093750e+00, 1.85118103027343750e+00, 1.85421752929687500e+00,
 1.85723876953125000e+00, 1.86026000976562500e+00, 1.86326599121093750e+00,
 1.86625671386718750e+00, 1.86924743652343750e+00, 1.87222290039062500e+00,
 1.87518310546875000e+00, 1.87814331054687500e+00, 1.88108825683593750e+00,
 1.88403320312500000e+00, 1.88696289062500000e+00, 1.88987731933593750e+00,
 1.89279174804687500e+00, 1.89569091796875000e+00, 1.89859008789062500e+00,
 1.90147399902343750e+00, 1.90435791015625000e+00, 1.90722656250000000e+00,
 1.91007995605468750e+00, 1.91293334960937500e+00, 1.91577148437500000e+00,
 1.91860961914062500e+00, 1.92143249511718750e+00, 1.92425537109375000e+00,
 1.92706298828125000e+00, 1.92985534667968750e+00, 1.93264770507812500e+00,
 1.93544006347656250e+00, 1.93821716308593750e+00, 1.94097900390625000e+00,
 1.94374084472656250e+00, 1.94650268554687500e+00, 1.94924926757812500e+00,
 1.95198059082031250e+00, 1.95471191406250000e+00, 1.95742797851562500e+00,
 1.96014404296875000e+00, 1.96286010742187500e+00, 1.96556091308593750e+00,
 1.96824645996093750e+00, 1.97093200683593750e+00, 1.97361755371093750e+00,
 1.97628784179687500e+00, 1.97894287109375000e+00, 1.98159790039062500e+00,
 1.98425292968750000e+00, 1.98689270019531250e+00, 1.98953247070312500e+00,
 1.99215698242187500e+00, 1.99478149414062500e+00, 1.99739074707031250e+00,
 2.00000000000000000e+00,
};

/*
 * The polynomial p(x) := p1*x + p2*x^2 + ... + p6*x^6 satisfies
 *
 * |(1+x)^(1/3) - 1 - p(x)| < 2^-63  for |x| < 0.003914
 */
static const double C[] = {
	 3.33333333333333340735623180707664400321413178600e-0001,
	-1.11111111111111111992797989129069515334791432304e-0001,
	 6.17283950578506695710302115234720605072083379082e-0002,
	-4.11522633731005164138964638666647311514892319010e-0002,
	 3.01788343105268728151735586597807324859173704847e-0002,
	-2.34723340038386971009665073968507263074215090751e-0002,
	18014398509481984.0
};

#define p1			C[0]
#define p2			C[1]
#define p3			C[2]
#define p4			C[3]
#define p5			C[4]
#define p6			C[5]
#define two54		C[6]

/* INDENT ON */

#if defined(__sparc)

#define HIWORD	0
#define LOWORD	1

#elif defined(__i386)

#define HIWORD	1
#define LOWORD	0

#else
#error Unknown architecture
#endif

#pragma weak cbrt = __cbrt

double __cbrt(double x)
{
	union {
		unsigned int	i[2];
		double			d;
	} xx, yy;
	double			t, u, w;
	unsigned int	hx, sx, ex, j, offset;

	xx.d = x;
	hx = xx.i[HIWORD] & ~0x80000000;
	sx = xx.i[HIWORD] & 0x80000000;

	/* handle special cases */
	if (hx >= 0x7ff00000) /* x is inf or nan */
#if defined(FPADD_TRAPS_INCOMPLETE_ON_NAN)
		return hx >= 0x7ff80000 ? x : x + x;
		/* assumes sparc-like QNaN */
#else
		return x + x;
#endif

	if (hx < 0x00100000) { /* x is subnormal or zero */
		if ((hx | xx.i[LOWORD]) == 0)
			return x;

		/* scale x to normal range */
		xx.d = x * two54;
		hx = xx.i[HIWORD] & ~0x80000000;
		offset = 0x29800000;
	}
	else
		offset = 0x2aa00000;

	ex = hx & 0x7ff00000;
	j = (ex >> 2) + (ex >> 4) + (ex >> 6);
	j = j + (j >> 6);
	j = 0x7ff00000 & (j + 0x2aa00); /* j is ex/3 */
	hx -= (j + j + j);
	xx.i[HIWORD] = 0x3ff00000 + hx;

	u = __libm_TBL_cbrt[(hx + 0x1000) >> 13];
	w = u * u * u;
	t = (xx.d - w) / w;

	yy.i[HIWORD] = sx | (j + offset);
	yy.i[LOWORD] = 0;

	w = t * t;
	return yy.d * (u + u * (t * (p1 + t * p2 + w * p3) +
		(w * w) * (p4 + t * p5 + w * p6)));
}