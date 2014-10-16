/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include "sqlite3.h"
#include "sqlite3util.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>


// Wrappers to sqlite operations.
// These will eventually get moved to commonlibs.

const char* enableWAL = {
	"PRAGMA journal_mode=WAL"
};

int sqlite3_prepare_statement(sqlite3* DB, sqlite3_stmt **stmt, const char* query, unsigned retries)
{
        int src = SQLITE_BUSY;

	for (unsigned i = 0; i < retries; i++) {
		src = sqlite3_prepare_v2(DB,query,strlen(query),stmt,NULL);
		if (src != SQLITE_BUSY && src != SQLITE_LOCKED) {
			break;
		}
		usleep(200);
	}
        if (src) {
                fprintf(stderr,"sqlite3_prepare_v2 failed for \"%s\": %s\n",query,sqlite3_errmsg(DB));
                sqlite3_finalize(*stmt);
        }
        return src;
}

int sqlite3_run_query(sqlite3* DB, sqlite3_stmt *stmt, unsigned retries)
{
	int src = SQLITE_BUSY;

        for (unsigned i = 0; i < retries; i++) {
                src = sqlite3_step(stmt);
		if (src != SQLITE_BUSY && src != SQLITE_LOCKED) {
                        break;
                }
                usleep(200);
        }
	if ((src!=SQLITE_DONE) && (src!=SQLITE_ROW)) {
		fprintf(stderr,"sqlite3_run_query failed: %s: %s\n", sqlite3_sql(stmt), sqlite3_errmsg(DB));
	}
	return src;
}


bool sqlite3_exists(sqlite3* DB, const char *tableName,
		const char* keyName, const char* keyData, unsigned retries)
{
	size_t stringSize = 100 + strlen(tableName) + strlen(keyName) + strlen(keyData);
	char query[stringSize];
	sprintf(query,"SELECT * FROM %s WHERE %s == \"%s\"",tableName,keyName,keyData);
	// Prepare the statement.
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_statement(DB,&stmt,query,retries)) return false;
	// Read the result.
	int src = sqlite3_run_query(DB,stmt,retries);
	sqlite3_finalize(stmt);
	// Anything there?
	return (src == SQLITE_ROW);
}



bool sqlite3_single_lookup(sqlite3* DB, const char *tableName,
		const char* keyName, const char* keyData,
		const char* valueName, unsigned &valueData, unsigned retries)
{
	size_t stringSize = 100 + strlen(valueName) + strlen(tableName) + strlen(keyName) + strlen(keyData);
	char query[stringSize];
	sprintf(query,"SELECT %s FROM %s WHERE %s == \"%s\"",valueName,tableName,keyName,keyData);
	// Prepare the statement.
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_statement(DB,&stmt,query,retries)) return false;
	// Read the result.
	int src = sqlite3_run_query(DB,stmt,retries);
	bool retVal = false;
	if (src == SQLITE_ROW) {
		valueData = (unsigned)sqlite3_column_int64(stmt,0);
		retVal = true;
	}
	sqlite3_finalize(stmt);
	return retVal;
}


// This function returns an allocated string that must be free'd by the caller.
bool sqlite3_single_lookup(sqlite3* DB, const char* tableName,
		const char* keyName, const char* keyData,
		const char* valueName, char* &valueData, unsigned retries)
{
	valueData=NULL;
	size_t stringSize = 100 + strlen(valueName) + strlen(tableName) + strlen(keyName) + strlen(keyData);
	char query[stringSize];
	sprintf(query,"SELECT %s FROM %s WHERE %s == \"%s\"",valueName,tableName,keyName,keyData);
	// Prepare the statement.
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_statement(DB,&stmt,query,retries)) return false;
	// Read the result.
	int src = sqlite3_run_query(DB,stmt,retries);
	bool retVal = false;
	if (src == SQLITE_ROW) {
		const char* ptr = (const char*)sqlite3_column_text(stmt,0);
		if (ptr) valueData = strdup(ptr);
		retVal = true;
	}
	sqlite3_finalize(stmt);
	return retVal;
}


// This function returns an allocated string that must be free'd by tha caller.
bool sqlite3_single_lookup(sqlite3* DB, const char* tableName,
		const char* keyName, unsigned keyData,
		const char* valueName, char* &valueData, unsigned retries)
{
	valueData=NULL;
	size_t stringSize = 100 + strlen(valueName) + strlen(tableName) + strlen(keyName) + 20;
	char query[stringSize];
	sprintf(query,"SELECT %s FROM %s WHERE %s == %u",valueName,tableName,keyName,keyData);
	// Prepare the statement.
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_statement(DB,&stmt,query,retries)) return false;
	// Read the result.
	int src = sqlite3_run_query(DB,stmt,retries);
	bool retVal = false;
	if (src == SQLITE_ROW) {
		const char* ptr = (const char*)sqlite3_column_text(stmt,0);
		if (ptr) valueData = strdup(ptr);
		retVal = true;
	}
	sqlite3_finalize(stmt);
	return retVal;
}




bool sqlite3_command(sqlite3* DB, const char* query, unsigned retries)
{
	// Prepare the statement.
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_statement(DB,&stmt,query,retries)) return false;
	// Run the query.
	int src = sqlite3_run_query(DB,stmt,retries);
	sqlite3_finalize(stmt);
	return (src==SQLITE_DONE || src==SQLITE_OK || src==SQLITE_ROW);
}



