/*dbStaticNoRun.c*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 * Modification Log:
 * -----------------
 * .01	06-JUN-95	mrk	Initial Version
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include <dbDefs.h>
#include <dbFldTypes.h>
#include <epicsPrint.h>
#include <errMdef.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>

long dbAllocRecord(DBENTRY *pdbentry,char *precordName)
{
    dbRecordType		*pdbRecordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes		*pflddes;
    void		**papField;
    int			i;
    char		*pstr;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    precnode->precord = dbCalloc(pdbRecordType->no_fields,sizeof(void *));
    papField = (void **)precnode->precord;
    for(i=0; i<pdbRecordType->no_fields; i++) {
	pflddes = pdbRecordType->papFldDes[i];
	if(!pflddes) continue;
	switch(pflddes->field_type) {
	case DBF_STRING:
	    if(pflddes->size <= 0) {
		fprintf(stderr,"size=0 for %s.%s\n",pdbRecordType->name,pflddes->name);
		pflddes->size = 1;
	    }
	    papField[i] = dbCalloc(pflddes->size,sizeof(char));
	    if(pflddes->initial)  {
		if(strlen(pflddes->initial) >= (unsigned)(pflddes->size)) {
		    fprintf(stderr,"initial size > size for %s.%s\n",
			pdbRecordType->name,pflddes->name);
		} else {
		    strcpy((char *)papField[i],pflddes->initial);
		}
	    }
	    break;
	case DBF_CHAR:
	case DBF_UCHAR:
	case DBF_SHORT:
	case DBF_USHORT:
	case DBF_LONG:
	case DBF_ULONG:
	case DBF_FLOAT:
	case DBF_DOUBLE:
	case DBF_ENUM:
	    if(pflddes->initial) {
		papField[i] =
			dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
		strcpy((char *)papField[i],pflddes->initial);
	    }
	    break;
	case DBF_MENU:
	case DBF_DEVICE:
	    papField[i] = dbCalloc(1,sizeof(unsigned short));
	    if(pflddes->initial) sscanf(pflddes->initial,"%hu",papField[i]);
	    break;
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		struct link *plink;

		papField[i] = plink = dbCalloc(1,sizeof(struct link));
		if(pflddes->initial) {
		    plink->value.constantStr =
			dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
		    strcpy(plink->value.constantStr,pflddes->initial);
		}
	    }
	    break;
	case DBF_NOACCESS:
	    break;
	default:
	    fprintf(stderr,"dbAllocRecord: Illegal field type\n");
	}
    }
    pstr = (char *)papField[0];
    strcpy(pstr,precordName);
    return(0);
}

long dbFreeRecord(DBENTRY *pdbentry)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;
    int		i,field_type;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    pap = (void **)precnode->precord;
    precnode->precord = NULL;
    for(i=0; i<pdbRecordType->no_fields; i++) {
	pflddes = pdbRecordType->papFldDes[i];
	field_type = pflddes->field_type;
	if(field_type==DBF_INLINK
	|| field_type==DBF_OUTLINK
	|| field_type==DBF_FWDLINK)
		dbFreeLinkContents((struct link *)pap[i]);
	free(pap[i]);
    }
    free((void *)pap);
    return(0);
}

long dbGetFieldAddress(DBENTRY *pdbentry)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!precnode->precord) return(0);
    pap = (void **)precnode->precord;
    pdbentry->pfield = pap[pflddes->indRecordType];
    return(0);
}

char *dbRecordName(DBENTRY *pdbentry)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    void	**pap;

    if(!pdbRecordType) return(0);
    if(!precnode) return(0);
    if(!precnode->precord) return(0);
    pap = (void **)precnode->precord;
    return((char *)pap[0]);
}

int  dbIsDefaultValue(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void       	*pfield = pdbentry->pfield;
    char	*endp;

    if(!pflddes) return(FALSE);
    switch (pflddes->field_type) {
	case DBF_STRING: 
	    if(!pfield) return(FALSE);
	    if(!pflddes->initial)
		return((*(char *)pfield =='\0') ? TRUE : FALSE);
	    return(strcmp((char *)pfield,(char *)pflddes->initial)==0);
	case DBF_CHAR:
	case DBF_UCHAR:
	case DBF_SHORT:
	case DBF_USHORT:
	case DBF_LONG:
	case DBF_ULONG:
	case DBF_FLOAT:
	case DBF_DOUBLE:
	case DBF_ENUM:
	    if(!pfield) return(TRUE);
	    if(!pflddes->initial) {
		if((strtod((char *)pfield,&endp)==0.0) && (*endp=='\0'))
			return(TRUE);
		return(FALSE);
	    }
	    return(strcmp((char *)pfield,(char *)pflddes->initial)==0);
	case DBF_MENU: {
		unsigned short val=0;
		unsigned short ival;

		if(pfield) val = *(unsigned short *)pfield;
		if(!pfield || pflddes->initial == 0)
		    return((val==0)?TRUE:FALSE);
		if(pflddes->initial == 0) return((val==0)?TRUE:FALSE);
		sscanf(pflddes->initial,"%hu",&ival);
		return((val==ival)?TRUE:FALSE);
	    }
	case DBF_DEVICE: {
		dbRecordType	*pdbRecordType = pdbentry->precordType;
		devSup		*pdevSup;

		if(!pdbRecordType) {
		    epicsPrintf("dbIsDefaultValue: pdbRecordType is NULL??\n");
		    return(FALSE);
		}
		pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
		if(!pdevSup) return(TRUE);
		return(FALSE);
	    }
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		struct link *plink = (struct link *)pfield;

		if(!plink) return(FALSE);
		if(plink->type!=CONSTANT) return(FALSE);
		if(!plink->value.constantStr) return(TRUE);
		if(!pflddes->initial) {
		    if((strtod((char *)plink->value.constantStr,&endp)==0.0)
		    && (*endp=='\0')) return(TRUE);
		    return(FALSE);
		}
		if(strcmp(plink->value.constantStr,pflddes->initial)==0)
		   return(TRUE);
		return(FALSE);
	    }
    }
    return(FALSE);
}

char *dbGetStringNum(DBENTRY *pdbentry)
{
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    void	**pap;
    static char	zero[] = "0";

    if(!precnode) return(0);
    if(!precnode->precord) return(0);
    if(!pflddes) return(0);
    pap = (void **)precnode->precord;
    if(!pfield) return(zero);
    return((char *)pap[pflddes->indRecordType]);
}

long dbPutStringNum(DBENTRY *pdbentry,char *pstring)
{
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes  	*pflddes = pdbentry->pflddes;
    char	*pfield = (char *)pdbentry->pfield;
    void	**pap;

    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(pfield) {
	if((unsigned)strlen(pfield) < (unsigned)strlen(pstring)) {
	    free((void *)pfield);
	    pfield = NULL;
	}
    }
    if(!pfield)  {
	pfield = dbCalloc(strlen(pstring)+1,sizeof(char));
	strcpy(pfield,pstring);
	pdbentry->pfield = pfield;
	pap = (void **)precnode->precord;
	pap[pflddes->indRecordType] = pfield;
    }
    strcpy(pfield,pstring);
    return(0);
}

void dbGetRecordtypeSizeOffset(dbRecordType *pdbRecordType)
{
    /*For no run cant and dont need to set size and offset*/
    return;
}
