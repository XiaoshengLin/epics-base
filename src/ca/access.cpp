
/*
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeffrey O. Hill
 *
 */

/*
 * allocate error message string array 
 * here so I can use sizeof
 */
#define CA_ERROR_GLBLSOURCE

/*
 * allocate header version strings here
 */
#define CAC_VERSION_GLOBAL

#include "iocinf.h"
#include "oldAccess.h"
#include "cac_IL.h"
#include "baseNMIU_IL.h"

epicsThreadPrivateId caClientContextId;

static epicsThreadOnceId caClientContextIdOnce = EPICS_THREAD_ONCE_INIT;

extern "C" void ca_client_exit_handler ()
{
    if ( caClientContextId ) {
        epicsThreadPrivateDelete ( caClientContextId );
        caClientContextId = 0;
    }
}

// runs once only for each process
extern "C" void ca_init_client_context ( void * )
{
    caClientContextId = epicsThreadPrivateCreate ();
    if ( caClientContextId ) {
        atexit ( ca_client_exit_handler );
    }
}

/*
 * fetchClientContext ();
 */
int fetchClientContext ( cac **ppcac )
{
    if ( caClientContextId == 0 ) {
        epicsThreadOnce ( &caClientContextIdOnce, ca_init_client_context, 0 );
        if ( caClientContextId == 0 ) {
            return ECA_ALLOCMEM;
        }
    }

    int status;
    *ppcac = ( cac * ) epicsThreadPrivateGet ( caClientContextId );
    if ( *ppcac ) {
        status = ECA_NORMAL;
    }
    else {
        status = ca_task_initialize ();
        if ( status == ECA_NORMAL ) {
            *ppcac = (cac *) epicsThreadPrivateGet ( caClientContextId );
            if ( ! *ppcac ) {
                status = ECA_INTERNAL;
            }
        }
    }

    return status;
}

/*
 *  Default Exception Handler
 */
extern "C" void ca_default_exception_handler ( struct exception_handler_args args )
{
    if ( args.chid && args.op != CA_OP_OTHER ) {
        ca_signal_formated (
            args.stat, 
            args.pFile, 
            args.lineNo, 
            "%s - with request chan=%s op=%ld data type=%s count=%ld",
            args.ctx,
            ca_name ( args.chid ),
            args.op,
            dbr_type_to_text ( args.type ),
            args.count );
    }
    else {
        ca_signal_formated (
            args.stat, 
            args.pFile, 
            args.lineNo, 
            args.ctx );
    }
}

/*
 *  ca_task_initialize ()
 */
extern "C" int epicsShareAPI ca_task_initialize ( void )
{
    return ca_context_create ( false );
}

extern "C" epicsShareFunc int epicsShareAPI ca_context_create ( int preemptiveCallBackEnable )
{
    cac *pcac;

    epicsThreadOnce ( &caClientContextIdOnce, ca_init_client_context, 0);

    if ( caClientContextId == 0 ) {
        return ECA_ALLOCMEM;
    }

    pcac = (cac *) epicsThreadPrivateGet ( caClientContextId );
	if ( pcac ) {
		return ECA_NORMAL;
	}

    pcac = new cac ( preemptiveCallBackEnable ? true : false );
	if ( ! pcac ) {
		return ECA_ALLOCMEM;
	}

    epicsThreadPrivateSet ( caClientContextId, (void *) pcac );

    return ECA_NORMAL;
}

//
// ca_register_service ()
//
epicsShareFunc int epicsShareAPI ca_register_service ( cacServiceIO *pService )
{
    cac *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }
    if ( pService ) {
        pcac->registerService ( *pService );
    }
    return ECA_NORMAL;
}

//
// ca_modify_host_name ()
//
// defunct
//
extern "C" int epicsShareAPI ca_modify_host_name ( const char * )
{
    return ECA_NORMAL;
}

//
// ca_modify_user_name()
//
// defunct
//
extern "C" int epicsShareAPI ca_modify_user_name ( const char * )
{
    return ECA_NORMAL;
}

//
// ca_context_destroy ()
//
extern "C" epicsShareFunc int epicsShareAPI ca_context_destroy ()
{
    cac   *pcac;

    if ( caClientContextId != NULL ) {
        pcac = (cac *) epicsThreadPrivateGet ( caClientContextId );
        if ( pcac ) {
            delete pcac;
            epicsThreadPrivateSet ( caClientContextId, 0 );
        }
    }

    return ECA_NORMAL;
}

/*
 *  ca_task_exit()
 *
 *  releases all resources alloc to a channel access client
 */
extern "C" epicsShareFunc int epicsShareAPI ca_task_exit ()
{
    return ca_context_destroy ();
}

/*
 *
 *      CA_BUILD_AND_CONNECT
 *
 *      backwards compatible entry point to ca_search_and_connect()
 */
extern "C" int epicsShareAPI ca_build_and_connect ( const char *name_str, chtype get_type,
            unsigned long get_count, chid * chan, void *pvalue, 
            caCh *conn_func, void *puser )
{
    if ( get_type != TYPENOTCONN && pvalue != 0 && get_count != 0 ) {
        return ECA_ANACHRONISM;
    }

    return ca_search_and_connect ( name_str, chan, conn_func, puser );
}

/*
 *  ca_search_and_connect() 
 */
extern "C" int epicsShareAPI ca_search_and_connect (
    const char *name_str, chid *chanptr,
    caCh *conn_func, void *puser )
{
    cac *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( name_str == NULL || *name_str == '\0' ) {
        return ECA_EMPTYSTR;
    }

    oldChannelNotify *pChanNotify = new oldChannelNotify ( conn_func, puser );
    if ( ! pChanNotify ) {
        return ECA_ALLOCMEM;
    }

    cacChannelIO *pIO = pcac->createChannelIO ( name_str, *pChanNotify );
    if ( pIO ) {
        *chanptr = pIO;
        // make sure that their chan pointer is set prior to
        // calling connection call backs
        pIO->initiateConnect ();
        return ECA_NORMAL;
    }
    else {
        pChanNotify->release ();
        return ECA_ALLOCMEM;
    }
}


/*
 * ca_array_get ()
 */
extern "C" int epicsShareAPI ca_array_get ( chtype type, 
            unsigned long count, chid pChan, void *pValue )
{
    cac *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    getCopy *pNotify = new getCopy ( *pcac, tmpType, count, pValue );
    if ( ! pNotify ) {
        return ECA_ALLOCMEM;
    }

    int status = pChan->read ( type, count, *pNotify );
    if ( status != ECA_NORMAL ) {
        pNotify->release ();
    }

    return status;
}

/*
 * ca_array_get_callback ()
 */
extern "C" int epicsShareAPI ca_array_get_callback ( chtype type, 
            unsigned long count, chid pChan,
            caEventCallBackFunc *pfunc, void *arg )
{
    getCallback *pNotify = new getCallback ( pfunc, arg );
    if ( ! pNotify ) {
        return ECA_ALLOCMEM;
    }

    int status = pChan->read ( type, count, *pNotify );
    if ( status != ECA_NORMAL ) {
        pNotify->release ();
    }

    return status;
}

/*
 *  ca_array_put_callback ()
 */
extern "C" int epicsShareAPI ca_array_put_callback ( chtype type, unsigned long count, 
    chid pChan, const void *pvalue, caEventCallBackFunc *pfunc, void *usrarg )
{
    putCallback *pNotify = new putCallback ( pfunc, usrarg );
    if ( ! pNotify ) {
        return ECA_ALLOCMEM;
    }

    int status = pChan->write ( type, count, pvalue, *pNotify );
    if ( status != ECA_NORMAL ) {
        pNotify->release ();
    }
    return status;
}

/*
 *  ca_array_put ()
 */
extern "C" int epicsShareAPI ca_array_put ( chtype type, unsigned long count, 
                                chid pChan, const void *pvalue )
{
     return pChan->write ( type, count, pvalue );
}

/*
 *  Specify an event subroutine to be run for connection events
 */
extern "C" int epicsShareAPI ca_change_connection_event ( chid pChan, caCh *pfunc )
{
    oldChannelNotify * pNotify = pChan->notify ().pOldChannelNotify ();
    if ( pNotify ) {
        return pNotify->changeConnCallBack ( *pChan, pfunc );
    }
    else {
        return ECA_INTERNAL;
    }
}

/*
 * ca_replace_access_rights_event
 */
extern "C" int epicsShareAPI ca_replace_access_rights_event ( chid pChan, caArh *pfunc )
{
    oldChannelNotify * pNotify = pChan->notify ().pOldChannelNotify ();
    if ( pNotify ) {
        return pNotify->replaceAccessRightsEvent ( *pChan, pfunc );
    }
    else {
        return ECA_INTERNAL;
    }

}

/*
 *  Specify an event subroutine to be run for asynch exceptions
 */
extern "C" int epicsShareAPI ca_add_exception_event ( caExceptionHandler *pfunc, void *arg )
{
    cac *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }
    
    pcac->changeExceptionEvent ( pfunc, arg );

    return ECA_NORMAL;
}

/*
 *  ca_add_masked_array_event
 */
extern "C" int epicsShareAPI ca_add_masked_array_event ( 
        chtype type, unsigned long count, chid pChan, 
        caEventCallBackFunc *pCallBack, void *pCallBackArg, 
        ca_real, ca_real, ca_real, 
        evid *monixptr, long mask )
{
    static const long maskMask = 0xffff;

    if ( INVALID_DB_REQ (type) ) {
        return ECA_BADTYPE;
    }

    if ( pCallBack == NULL ) {
        return ECA_BADFUNCPTR;
    }

    if ( ( mask & maskMask ) == 0) {
        return ECA_BADMASK;
    }

    if ( mask & ~maskMask ) {
        return ECA_BADMASK;
    }

    /*
     * Check for huge waveform
     *
     * (the count is not checked here against the native count
     * when connected because this introduces a race condition 
     * for the client tool - the requested count is clipped to 
     * the actual count when the monitor request is sent so
     * verifying that the requested count is valid here isnt
     * required)
     */
    if ( dbr_size_n ( type, count ) > MAX_MSG_SIZE - sizeof ( caHdr ) ) {
        return ECA_TOLARGE;
    }

    oldSubscription *pSubsr = new oldSubscription  ( pCallBack, pCallBackArg );
    if ( ! pSubsr ) {
        return ECA_ALLOCMEM;
    }

    cacNotifyIO *pIO;
    int status = pChan->subscribe ( type, count, mask, *pSubsr, pIO );
    if ( status == ECA_NORMAL ) {
        if ( monixptr ) {
            *monixptr = pIO;
        }
    }
    else {
        pSubsr->release ();
    }

    return status;
}

/*
 *  ca_clear_event ()
 */
extern "C" int epicsShareAPI ca_clear_event ( evid pMon )
{
    pMon->cancel ();
    return ECA_NORMAL;
}

extern "C" chid epicsShareAPI ca_evid_to_chid ( evid pMon )
{
    return & pMon->channelIO ();
}

/*
 *  ca_clear_channel ()
 */
extern "C" int epicsShareAPI ca_clear_channel ( chid pChan )
{
    delete pChan;
    return ECA_NORMAL;
}

/*
 * ca_pend_event ()
 */
extern "C" int epicsShareAPI ca_pend_event (ca_real timeout)
{
    cac *pcac;
    int status = fetchClientContext ( &pcac );
    if ( status != ECA_NORMAL ) {
        return status;
    }

    return pcac->pendEvent ( timeout );
}

/*
 * ca_pend_io ()
 */
extern "C" int epicsShareAPI ca_pend_io (ca_real timeout)
{
    cac *pcac;
    int status = fetchClientContext ( &pcac );
    if ( status != ECA_NORMAL ) {
        return status;
    }

    return pcac->pendIO ( timeout );
}

/*
 *  ca_flush_io ()
 */ 
extern "C" int epicsShareAPI ca_flush_io ()
{
    cac *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->flushRequest ();

    return ECA_NORMAL;
}

/*
 *  CA_TEST_IO ()
 */
extern "C" int epicsShareAPI ca_test_io ()
{
    cac *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( pcac->ioComplete () ) {
        return ECA_IODONE;
    }
    else{
        return ECA_IOINPROGRESS;
    }
}

/*
 *  CA_SIGNAL()
 */
extern "C" void epicsShareAPI ca_signal ( long ca_status, const char *message )
{
    ca_signal_with_file_and_lineno ( ca_status, message, NULL, 0 );
}

/*
 * ca_message (long ca_status)
 *
 * - if it is an unknown error code then it possible
 * that the error string generated below 
 * will be overwritten before (or while) the caller
 * of this routine is calling this routine
 * (if they call this routine again).
 */
extern "C" const char * epicsShareAPI ca_message ( long ca_status )
{
    unsigned msgNo = CA_EXTRACT_MSG_NO ( ca_status );

    if ( msgNo < NELEMENTS (ca_message_text) ) {
        return ca_message_text[msgNo];
    }
    else {
        return "new CA message number known only by server - see caerr.h";
    }
}

/*
 * ca_signal_with_file_and_lineno()
 */
extern "C" void epicsShareAPI ca_signal_with_file_and_lineno ( long ca_status, 
            const char *message, const char *pfilenm, int lineno )
{
    ca_signal_formated ( ca_status, pfilenm, lineno, message );
}

/*
 * ca_signal_formated()
 */
extern "C" void epicsShareAPI ca_signal_formated ( long ca_status, const char *pfilenm, 
                                       int lineno, const char *pFormat, ... )
{
    cac *pcac;
    va_list theArgs;
    static const char *severity[] = 
    {
        "Warning",
        "Success",
        "Error",
        "Info",
        "Fatal",
        "Fatal",
        "Fatal",
        "Fatal"
    };

    if ( caClientContextId ) {
        pcac = (cac *) epicsThreadPrivateGet ( caClientContextId );
    }
    else {
        pcac = NULL;
    }
    
    va_start ( theArgs, pFormat );  
    
    pcac->printf ( "CA.Client.Diagnostic..............................................\n" );
    
    pcac->printf ( "    %s: \"%s\"\n", 
        severity[ CA_EXTRACT_SEVERITY ( ca_status ) ], 
        ca_message ( ca_status ) );

    if  ( pFormat ) {
        pcac->printf ( "    Context: \"" );
        pcac->vPrintf ( pFormat, theArgs );
        pcac->printf ( "\"\n" );
    }
        
    if (pfilenm) {
        pcac->printf ( "    Source File: %s Line Number: %d\n",
            pfilenm, lineno );    
    }
    
    /*
     *  Terminate execution if unsuccessful
     */
    if( ! ( ca_status & CA_M_SUCCESS ) && 
        CA_EXTRACT_SEVERITY ( ca_status ) != CA_K_WARNING ){
        errlogFlush();
        abort();
    }
    
    pcac->printf ( "..................................................................\n" );
    
    va_end ( theArgs );
}

/*
 *  CA_ADD_FD_REGISTRATION
 *
 *  call their function with their argument whenever 
 *  a new fd is added or removed
 *  (for a manager of the select system call under UNIX)
 *
 */
extern "C" int epicsShareAPI ca_add_fd_registration (CAFDHANDLER *func, void *arg)
{
    cac *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->registerForFileDescriptorCallBack ( func, arg );

    return ECA_NORMAL;
}

/*
 * ca_get_host_name ()
 */
extern "C" void epicsShareAPI ca_get_host_name ( chid pChan, char *pBuf, unsigned bufLength )
{
    pChan->hostName ( pBuf, bufLength );
}

/*
 * ca_host_name ()
 */
extern "C" const char * epicsShareAPI ca_host_name ( chid pChan )
{
    return pChan->pHostName ();
}

/*
 * ca_v42_ok(chid chan)
 */
extern "C" int epicsShareAPI ca_v42_ok (chid pChan)
{
    return pChan->ca_v42_ok ();
}

/*
 * ca_version()
 * function that returns the CA version string
 */
extern "C" const char * epicsShareAPI ca_version()
{
    return CA_VERSION_STRING; 
}

/*
 * ca_replace_printf_handler ()
 */
extern "C" int epicsShareAPI ca_replace_printf_handler (caPrintfFunc *ca_printf_func)
{
    cac *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->replaceErrLogHandler ( ca_printf_func );

    return ECA_NORMAL;
}

/*
 * ca_printf()
 */
int ca_printf ( const char *pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = ca_vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

/*
 * ca_vPrintf()
 */
int ca_vPrintf ( const char *pformat, va_list args )
{
    if ( caClientContextId ) {
        cac *pcac = (cac *) epicsThreadPrivateGet ( caClientContextId );
        if ( pcac ) {
            return pcac->vPrintf ( pformat, args );
        }
        else {
            return errlogVprintf ( pformat, args );
        }
    }
    else {
        return errlogVprintf ( pformat, args );
    }
}


/*
 * ca_field_type()
 */
extern "C" short epicsShareAPI ca_field_type ( chid pChan ) 
{
    return pChan->nativeType ();
}

/*
 * ca_element_count ()
 */
extern "C" unsigned long epicsShareAPI ca_element_count (chid pChan) 
{
    return pChan->nativeElementCount ();
}

/*
 * ca_state ()
 */
extern "C" epicsShareFunc enum channel_state epicsShareAPI ca_state (chid pChan)
{
    return pChan->state ();
}

/*
 * ca_set_puser ()
 */
extern "C" epicsShareFunc void epicsShareAPI ca_set_puser (chid pChan, void *puser)
{
    oldChannelNotify *pNotify = pChan->notify ().pOldChannelNotify ();
    pNotify->setPrivatePointer ( puser );
}

/*
 * ca_get_puser ()
 */
extern "C" epicsShareFunc void * epicsShareAPI ca_puser (chid pChan)
{
    oldChannelNotify *pNotify = pChan->notify ().pOldChannelNotify ();
    return pNotify->privatePointer ();
}

/*
 * ca_read_access ()
 */
extern "C" epicsShareFunc unsigned epicsShareAPI ca_read_access (chid pChan)
{
    if ( pChan->accessRights ().read_access ) {
        return true;
    }
    else {
        return false;
    }
}

/*
 * ca_write_access ()
 */
extern "C" epicsShareFunc unsigned epicsShareAPI ca_write_access (chid pChan)
{
    if ( pChan->accessRights ().write_access ) {
        return true;
    }
    else {
        return false;
    }
}

/*
 * ca_name ()
 */
extern "C" epicsShareFunc const char * epicsShareAPI ca_name ( chid pChan )
{
    return pChan->pName ();
}

extern "C" epicsShareFunc unsigned epicsShareAPI ca_search_attempts ( chid pChan )
{
    return pChan->searchAttempts ();
}

extern "C" epicsShareFunc double epicsShareAPI ca_beacon_period ( chid pChan )
{
    return pChan->beaconPeriod ();
}

/*
 * ca_get_ioc_connection_count()
 *
 * returns the number of IOC's that CA is connected to
 * (for testing purposes only)
 */
extern "C" unsigned epicsShareAPI ca_get_ioc_connection_count () 
{
    cac *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    return pcac->connectionCount ();
}

extern "C" epicsShareFunc int epicsShareAPI ca_channel_status ( epicsThreadId /* tid */ )
{
    printf ("new OSI API does not allow peeking at thread private storage of another thread\n");
    printf ("please call \"ca_client_status ( unsigned level )\" from the subsystem specific diagnostic code.\n");
	return ECA_ANACHRONISM;
}

extern "C" epicsShareFunc int epicsShareAPI ca_client_status ( unsigned level )
{
    cac *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->show ( level );
    return ECA_NORMAL;
}

/*
 * ca_current_context ()
 *
 * used when an auxillary thread needs to join a CA client context started
 * by another thread
 */
extern "C" epicsShareFunc int epicsShareAPI ca_current_context ( caClientCtx *pCurrentContext )
{
    if ( caClientContextId ) {
        void *pCtx = epicsThreadPrivateGet ( caClientContextId );
        if (pCtx) {
            *pCurrentContext = pCtx;
            return ECA_NORMAL;
        }
        else {
            return ECA_NOCACTX;
        }
    }
    else {
        return ECA_NOCACTX;
    }
}

/*
 * ca_attach_context ()
 *
 * used when an auxillary thread needs to join a CA client context started
 * by another thread
 */
extern "C" epicsShareFunc int epicsShareAPI ca_attach_context ( caClientCtx context )
{
    cac *pcac;

    pcac = (cac *) epicsThreadPrivateGet ( caClientContextId );
    if ( pcac && context != 0 ) {
        return ECA_ISATTACHED;
    }
    epicsThreadPrivateSet ( caClientContextId, context );
    return ECA_NORMAL;
}

extern "C" epicsShareDef const int epicsTypeToDBR_XXXX [lastEpicsType+1] = {
    DBR_SHORT, /* forces conversion fronm uint8 to int16 */
    DBR_CHAR,
    DBR_SHORT,
    DBR_LONG, /* forces conversion fronm uint16 to int32 */
    DBR_ENUM,
    DBR_LONG,
    DBR_LONG, /* very large unsigned number will not map */
    DBR_FLOAT,
    DBR_DOUBLE,
    DBR_STRING,
    DBR_STRING
};

extern "C" epicsShareDef const epicsType DBR_XXXXToEpicsType [LAST_BUFFER_TYPE+1] = {
	epicsOldStringT,
	epicsInt16T,	
	epicsFloat32T,	
	epicsEnum16T,	
	epicsUInt8T,	
	epicsInt32T,	
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,	
	epicsFloat32T,	
	epicsEnum16T,	
	epicsUInt8T,	
	epicsInt32T,	
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,	
	epicsFloat32T,	
	epicsEnum16T,	
	epicsUInt8T,	
	epicsInt32T,	
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,	
	epicsFloat32T,	
	epicsEnum16T,	
	epicsUInt8T,	
	epicsInt32T,	
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,	
	epicsFloat32T,	
	epicsEnum16T,	
	epicsUInt8T,	
	epicsInt32T,	
	epicsFloat64T,

	epicsUInt16T,
	epicsUInt16T,
	epicsOldStringT,
	epicsOldStringT
};

extern "C" epicsShareDef const unsigned short dbr_size[LAST_BUFFER_TYPE+1] = {
	sizeof(dbr_string_t),		/* string max size		*/
	sizeof(dbr_short_t),		/* short			*/
	sizeof(dbr_float_t),		/* IEEE Float			*/
	sizeof(dbr_enum_t),		/* item number			*/
	sizeof(dbr_char_t),		/* character			*/
	sizeof(dbr_long_t),		/* long				*/
	sizeof(dbr_double_t),		/* double			*/
	sizeof(struct dbr_sts_string),	/* string field	with status	*/
	sizeof(struct dbr_sts_short),	/* short field with status	*/
	sizeof(struct dbr_sts_float),	/* float field with status	*/
	sizeof(struct dbr_sts_enum),	/* item number with status	*/
	sizeof(struct dbr_sts_char),	/* char field with status	*/
	sizeof(struct dbr_sts_long),	/* long field with status	*/
	sizeof(struct dbr_sts_double),	/* double field with time	*/
	sizeof(struct dbr_time_string),	/* string field	with time	*/
	sizeof(struct dbr_time_short),	/* short field with time	*/
	sizeof(struct dbr_time_float),	/* float field with time	*/
	sizeof(struct dbr_time_enum),	/* item number with time	*/
	sizeof(struct dbr_time_char),	/* char field with time		*/
	sizeof(struct dbr_time_long),	/* long field with time		*/
	sizeof(struct dbr_time_double),	/* double field with time	*/
	sizeof(struct dbr_sts_string),	/* graphic string info		*/
	sizeof(struct dbr_gr_short),	/* graphic short info		*/
	sizeof(struct dbr_gr_float),	/* graphic float info		*/
	sizeof(struct dbr_gr_enum),	/* graphic item info		*/
	sizeof(struct dbr_gr_char),	/* graphic char info		*/
	sizeof(struct dbr_gr_long),	/* graphic long info		*/
	sizeof(struct dbr_gr_double),	/* graphic double info		*/
	sizeof(struct dbr_sts_string),	/* control string info		*/
	sizeof(struct dbr_ctrl_short),	/* control short info		*/
	sizeof(struct dbr_ctrl_float),	/* control float info		*/
	sizeof(struct dbr_ctrl_enum),	/* control item info		*/
	sizeof(struct dbr_ctrl_char),	/* control char info		*/
	sizeof(struct dbr_ctrl_long),	/* control long info		*/
	sizeof(struct dbr_ctrl_double),	/* control double info		*/
	sizeof(dbr_put_ackt_t),		/* put ackt			*/
	sizeof(dbr_put_acks_t),		/* put acks			*/
	sizeof(struct dbr_stsack_string),/* string field with status/ack*/
	sizeof(dbr_string_t),		/* string max size		*/
};

extern "C" epicsShareDef const unsigned short dbr_value_size[LAST_BUFFER_TYPE+1] = {
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_ushort_t), 	/* put_ackt			*/
	sizeof(dbr_ushort_t), 	/* put_acks			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_string_t),	/* string max size		*/
};

extern "C" epicsShareDef const enum dbr_value_class dbr_value_class[LAST_BUFFER_TYPE+1] = {
	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/
	dbr_class_int,
	dbr_class_int,
	dbr_class_string,
	dbr_class_string,	/* string max size		*/
};

extern "C" epicsShareDef const unsigned short dbr_value_offset[LAST_BUFFER_TYPE+1] = {
	0,					/* string			*/
	0,					/* short			*/
	0,					/* IEEE Float			*/
	0,					/* item number			*/
	0,					/* character			*/
	0,					/* long				*/
	0,					/* IEEE double			*/
	offsetof(dbr_sts_string,value[0]),/* string field	with status	*/
	offsetof(dbr_sts_short,value),	/* short field with status	*/
	offsetof(dbr_sts_float,value),	/* float field with status	*/
	offsetof(dbr_sts_enum,value),	/* item number with status	*/
	offsetof(dbr_sts_char,value),	/* char field with status	*/
	offsetof(dbr_sts_long,value),	/* long field with status	*/
	offsetof(dbr_sts_double,value),	/* double field with time	*/
	offsetof(dbr_time_string,value[0]),/* string field with time	*/
	offsetof(dbr_time_short,value),	/* short field with time	*/
	offsetof(dbr_time_float,value),	/* float field with time	*/
	offsetof(dbr_time_enum,value),	/* item number with time	*/
	offsetof(dbr_time_char,value),	/* char field with time		*/
	offsetof(dbr_time_long,value),	/* long field with time		*/
	offsetof(dbr_time_double,value),	/* double field with time	*/
	offsetof(dbr_sts_string,value[0]),/* graphic string info		*/
	offsetof(dbr_gr_short,value),	/* graphic short info		*/
	offsetof(dbr_gr_float,value),	/* graphic float info		*/
	offsetof(dbr_gr_enum,value),	/* graphic item info		*/
	offsetof(dbr_gr_char,value),	/* graphic char info		*/
	offsetof(dbr_gr_long,value),	/* graphic long info		*/
	offsetof(dbr_gr_double,value),	/* graphic double info		*/
	offsetof(dbr_sts_string,value[0]),/* control string info		*/
	offsetof(dbr_ctrl_short,value),	/* control short info		*/
	offsetof(dbr_ctrl_float,value),	/* control float info		*/
	offsetof(dbr_ctrl_enum,value),	/* control item info		*/
	offsetof(dbr_ctrl_char,value),	/* control char info		*/
	offsetof(dbr_ctrl_long,value),	/* control long info		*/
	offsetof(dbr_ctrl_double,value),	/* control double info		*/
	0,					/* put ackt			*/
	0,					/* put acks			*/
	offsetof(dbr_stsack_string,value[0]),/* string field	with status	*/
	0,					/* string			*/
};

extern "C" epicsShareDef const char *dbf_text[LAST_TYPE+3] = {
	"TYPENOTCONN",
	"DBF_STRING",
	"DBF_SHORT",
	"DBF_FLOAT",
	"DBF_ENUM",
	"DBF_CHAR",
	"DBF_LONG",
	"DBF_DOUBLE",
	"DBF_NO_ACCESS"
};

extern "C" epicsShareDef const char *dbf_text_invalid = "DBF_invalid";

extern "C" epicsShareDef const short dbf_text_dim = (sizeof dbf_text)/(sizeof (char *));

extern "C" epicsShareDef const char *dbr_text[LAST_BUFFER_TYPE+1] = {
    "DBR_STRING",
    "DBR_SHORT",
    "DBR_FLOAT",
    "DBR_ENUM",
    "DBR_CHAR",
    "DBR_LONG",
    "DBR_DOUBLE",
    "DBR_STS_STRING",
    "DBR_STS_SHORT",
    "DBR_STS_FLOAT",
    "DBR_STS_ENUM",
    "DBR_STS_CHAR",
    "DBR_STS_LONG",
    "DBR_STS_DOUBLE",
    "DBR_TIME_STRING",
    "DBR_TIME_SHORT",
    "DBR_TIME_FLOAT",
    "DBR_TIME_ENUM",
    "DBR_TIME_CHAR",
    "DBR_TIME_LONG",
    "DBR_TIME_DOUBLE",
    "DBR_GR_STRING",
    "DBR_GR_SHORT",
    "DBR_GR_FLOAT",
    "DBR_GR_ENUM",
    "DBR_GR_CHAR",
    "DBR_GR_LONG",
    "DBR_GR_DOUBLE",
    "DBR_CTRL_STRING",
    "DBR_CTRL_SHORT",
    "DBR_CTRL_FLOAT",
    "DBR_CTRL_ENUM",
    "DBR_CTRL_CHAR",
    "DBR_CTRL_LONG",
    "DBR_CTRL_DOUBLE",
    "DBR_PUT_ACKT",
    "DBR_PUT_ACKS",
    "DBR_STSACK_STRING",
    "DBR_CLASS_NAME"
};

extern "C" epicsShareDef const char *dbr_text_invalid = "DBR_invalid";
extern "C" epicsShareDef const short dbr_text_dim = (sizeof dbr_text) / (sizeof (char *)) + 1;
