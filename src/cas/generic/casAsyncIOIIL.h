/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casAsyncIOIIL_h
#define casAsyncIOIIL_h

//
// void casAsyncIOI::lock()
//
inline void casAsyncIOI::lock()
{
	client.lock();
}

//
// void casAsyncIOI::unlock()
//
inline void casAsyncIOI::unlock()
{
	client.unlock();
}

//
// casAsyncIO * casAsyncIOI::operator -> ()
//
inline casAsyncIO * casAsyncIOI::operator -> ()
{
	return &this->asyncIO;
}

#endif // casAsyncIOIIL_h

