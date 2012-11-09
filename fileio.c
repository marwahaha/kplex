/* fileio.c
 * This file is part of kplex
 * Copyright Keith Young 2012
 * For copying information see the file COPYING distributed with this software
 *
 * This file contains code for i/o from files (incl stdin/stdout)
 */

#include "kplex.h"
#include <stdlib.h>

#define DEFFILEQSIZE 128

struct if_file {
    FILE *fp;
};

/*
 * Duplicate struct if_file
 * Args: if_file to be duplicated
 * Returns: pointer to new if_file
 */
void *ifdup_file(void *iff)
{
    struct if_file  *newif;

    if ((newif = (struct if_file *) malloc(sizeof(struct if_file)))
        == (struct if_file *) NULL)
        return(NULL);

    /* Read/Write only supported for stdin/stdout so don't allocate fp */
    return(newif);
}

void cleanup_file(iface_t *ifa)
{
    struct if_file *iff = (struct if_file *) ifa->info;

    fclose(iff->fp);
}

iface_t *write_file(iface_t *ifa)
{
    struct if_file *ifc = (struct if_file *) ifa->info;
    FILE *fp = ifc->fp;
    senblk_t *sptr;

    for(;;)  {
        if ((sptr = next_senblk(ifa->q)) == NULL) {
		    break;
        }
        sptr->data[sptr->len-2] = '\n';
        sptr->data[sptr->len-1] = '\0';
        if (fputs(sptr->data,fp) == EOF)
		break;
        senblk_free(sptr,ifa->q);
    }
    iface_destroy(ifa,(void *) &errno);
}

iface_t *read_file(iface_t *ifa)
{
    struct if_file *ifc = (struct if_file *) ifa->info;
    senblk_t sblk;
    int len;
    char *eptr;

    sblk.src=ifa;
    while (fgets(sblk.data,SENMAX,ifc->fp) == sblk.data) {
        if ((len = strlen(sblk.data)) == 0) {
            break;
        }
        if (sblk.data[len-1]  != '\n') {
            fprintf(stderr,"Error: Line exceeds max sentence length (discarding)\n");
            while ((eptr = fgets(sblk.data,SENMAX,ifc->fp)) == sblk.data) {
                if (sblk.data[strlen(sblk.data)-1]  == '\n') {
                    break;
                }
            }
            if (eptr == NULL)
                break;
            continue;
        }
        sblk.data[len-1]='\r';
        sblk.data[len]='\n';
        sblk.len=len+1;
        push_senblk(&sblk,ifa->q);
    }
    iface_destroy(ifa,(void *) &errno);
}

iface_t *init_file (iface_t *ifa)
{
    char *fname=NULL;
    size_t qsize=DEFFILEQSIZE;
    struct if_file *ifc;
    struct kopts *opt,*nextopt;

    if ((ifc = (struct if_file *)malloc(sizeof(struct if_file))) == NULL) {
        perror("Could not allocate memory");
        exit(1);
    }
    ifa->info = (void *) ifc;

    for(opt=ifa->options;opt;opt=opt->next) {
        if (!strcasecmp(opt->var,"filename"))
            fname=opt->val;
        else if (!strcasecmp(opt->var,"qsize")) {
            if (!(qsize=atoi(opt->val))) {
                fprintf(stderr,"Invalid queue size specified: %s",opt->val);
                exit(1);
            }
        } else  {
            fprintf(stderr,"unknown interface option %s\n",opt->var);
            exit(1);
        }
    }

    if (!fname) {
        fprintf(stderr,"Must specify a filename for file interfaces\n");
        exit(1);
    }
    if (!strcmp(fname,"-")) {
        ifc->fp = (ifa->direction == IN)?stdin:stdout;
    } else {
        if (ifa->direction == BOTH) {
            fprintf(stderr,"Bi-directional file I/O only supported for stdin/stdout\n");
            exit(1);
        }
        if ((ifc->fp = fopen(fname,(ifa->direction == IN)?"r":"w"))
                == NULL) {
            fprintf(stderr,"Could not open %s: %s\n",fname,strerror(errno));
            exit(1);
        }
    }

    if (ifa->direction != IN)
        if ((ifa->q =init_q(qsize)) == NULL) {
            perror("Could not create queue");
            exit(1);
        }

    free_options(ifa->options);

    ifa->write=write_file;
    ifa->read=read_file;
    ifa->cleanup=cleanup_file;
    if (ifa->direction == BOTH) {
        if ((ifa->next=ifdup(ifa)) == NULL) {
            perror("Interface duplication failed");
            exit(1);
        }
        ifa->direction=OUT;
        ifa->pair->direction=IN;
        ifc = (struct if_file *) ifa->pair->info;
        ifc->fp=stdin;
    }
    return(ifa);
}

