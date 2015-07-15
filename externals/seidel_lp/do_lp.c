#define REAL 0
#define PROJECTIVE 1

#include "lp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int read_lp(FLOAT **halves, int *d, int *m, FLOAT **n_vec, FLOAT **d_vec, int *itype);
int lp_d_unit(int d, FLOAT a[], FLOAT b[]);

int main(int argc,char *argv[])
{
	int *next, *prev;
	FLOAT *halves, *n_vec, *opt, *d_vec;
	int *perm, r;
	int m, d, i, j, k;
	int status = AMBIGUOUS, type;
	FLOAT *work;
	int repeat;

	if(argc>1) {
		repeat = atoi(argv[1]);
	} else {
		repeat = 1;
	}
	if(read_lp(&halves,&d,&m,&n_vec, &d_vec, &type)) {
		perm = (int*)malloc((unsigned)(m-1)*sizeof(int));
		next = (int*)malloc((unsigned)(m)*sizeof(int));
		prev = (int*)malloc((unsigned)(m)*sizeof(int));
		printf("dimension: %d, number of planes: %d, repeat: %d\n",
			d,m,repeat);
		opt = (FLOAT *)malloc((unsigned)(d+1)*sizeof(FLOAT));
		work = (FLOAT *)malloc((unsigned)(m+3)*(d+2)*(d-1)/2*sizeof(FLOAT));
		for(r=0; r<repeat; r++) {
/* randomize the input planes */
			randperm(m-1,perm);
/* previous to 0 should never be used */
			prev[0] = 1234567890;
/* link the zero position in at the beginning */
			next[0] = perm[0]+1;
			prev[perm[0]+1] = 0;
/* link the other planes */
			for(i=0; i<m-2; i++) {
				next[perm[i]+1] = perm[i+1]+1;
				prev[perm[i+1]+1] = perm[i]+1;
			}
/* flag the last plane */
			next[perm[m-2]+1] = m;
			status = linprog(halves,0,m,n_vec,d_vec,d,opt,work,
				next,prev,m);
		}
		switch(status) {
			case INFEASIBLE:
				(void)printf("no feasible solution\n");
				break;
			case MINIMUM:
				(void)printf("minimum attained at\n");
				break;
			case UNBOUNDED:
				(void)printf("region is unbounded: last vertex is\n");
				break;
			case AMBIGUOUS:
			(void)printf("region is bounded by plane orthogonal\n");
 			(void)printf("to minimization vector: one vertex is\n");
				break;
			default:
				(void)printf("unknown case returned from linprog\n");
		}
		if(status!=INFEASIBLE) {
			if(type == PROJECTIVE) {
				(void)printf("(");
				for(i=0; i<d; i++) {
					if(opt[d]==0.0) {
						(void)printf("%f",opt[i]);
					} else {
						(void)printf("%f",opt[i]/opt[d]);
					}
					if(i!=d-1)(void)printf(",");
				}
				(void)printf(")\n");
				if(opt[d]==0.0) (void)printf("at infinite\n");
				for(j=0; j!=m; j=next[j]) {
				    FLOAT val = 0.0;
			    for(k=0; k<=d; k++) val += opt[k]*halves[j*(d+1)+k];
                                    if(val <-d*EPS) {
                                        printf("error\n");
                                        exit(1);
                                    }
                                }
			} else {
				(void)printf("(");
				for(i=0; i<=d; i++) {
					(void)printf("%f",opt[i]);
					if(i!=d)(void)printf(",");
				}
				(void)printf(")\n");
			}
		}
		free((char *)halves);
		free((char *)next);
		free((char *)prev);
		free((char *)perm);
		free((char *)n_vec);
		free((char *)opt);
		free((char *)work);
	} else {
		(void)printf("parse error\n");
	}
	return 0;
}
#ifdef  DOUBLE
#define	CONVERSION	"%lf"
#else
#define CONVERSION	"%f"
#endif
int read_lp(FLOAT **halves, int *d, int *m, FLOAT **n_vec, FLOAT **d_vec, int *itype)
{
	int i, j;
	char type[100];

	if(scanf("dimension: %d, number of planes: %d\n",d,m) !=2)return(0);
	if((i=scanf("%s",type)) !=1)return(0);
	*n_vec = (FLOAT *)malloc((unsigned)(*d+1)*sizeof(FLOAT));
	*d_vec = (FLOAT *)malloc((unsigned)(*d+1)*sizeof(FLOAT));
	*halves = (FLOAT *)malloc((unsigned)*m*(*d+1)*sizeof(FLOAT));
	for(i=0; i<=*d; i++) {
		(*d_vec)[i] = 0.0;
	}
	if(!strcmp(type,"projective")) {
		*itype = PROJECTIVE;
		for(i=0; i<*d; i++) {
			if(scanf(CONVERSION,(*n_vec)+i)!=1)  goto err;
		}
		(*d_vec)[*d] = 1.0;
		(*n_vec)[*d] = 0.0;
	} else if(!strcmp(type,"real")){
		*itype = REAL;
		for(i=0; i<=*d; i++) {
			if(scanf(CONVERSION,(*n_vec)+i)!=1)  goto err;
		}
	} else {
		goto err;
	}
	for(i=0; i<*m; i++) {
		for(j=0; j<=*d; j++) {
			if(scanf(CONVERSION,(*halves)+i*(*d+1)+j)!=1) {
				goto err;
			}
		}
		(void)lp_d_unit(*d,(*halves) + i*(*d+1),(*halves) + i*(*d+1));
	}
	return(1);
 err: 	free((char *)*d_vec);
	free((char *)*n_vec);
	free((char *)*halves);
	return(0);
}
