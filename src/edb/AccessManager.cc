/*                                                                                          
 * AccessManager.cpp
 *                                                                                          
 *  Created on: April 24, 2011     
 *      Authors: cat_red                                                         
 */


#include "AccessManager.h"

#define PRINCTYPE "varchar(255)"
#define NODIGITS 4

//------------------------------------------------------------------------------------------
//returns true if e is in ls
bool inList(list<string> &ls, string e) {
	list<string>::iterator it;
	for(it = ls.begin(); it != ls.end(); it++) {
		if(it->compare(e) == 0) {
			return true;
		}
	}
	return false;
}

bool inList(list<Prin> &ls, Prin e) {
	list<Prin>::iterator it;
	for(it = ls.begin(); it != ls.end(); it++) {
		if(*it == e) {
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------------------

MetaAccess::MetaAccess(Connect * c, bool verb) {
	this->VERBOSE = verb;
	this->table_name = "cryptdb_";
	this->public_table = "cryptdb_initalized_principles";
	this->table_num = 0;
	this->conn = c;
}

string MetaAccess::publicTableName() {
	return this->public_table;
}

string MetaAccess::sanitize(string unsanitized) {
	assert_s(unsanitized.find(".") != string::npos, "input sanitize does not have '.' separator");
	int nodigits = unsanitized.find(".");

	string repr = marshallVal(nodigits, NODIGITS);
	assert_s(repr.length() <= NODIGITS, "given fieldname is longer than max allowed by pruning ");

	string result = repr + unsanitized.substr(0, nodigits) + unsanitized.substr(nodigits+1, unsanitized.length()-1-nodigits);

	return result;
};

string MetaAccess::unsanitize(string sanitized) {
	assert_s(sanitized.find(".") == string::npos, "input to unsanitize has '.' separator");
	unsigned int digits = 0;

	for (int i = 0; i < NODIGITS; i++) {
		assert_s(int(sanitized[i]-'0') < 10 && int(sanitized[i]-'0') >= 0, "the input to unsanitize does not begin with the correct number of digits");
		digits = digits*10 + (int)(sanitized[i]-'0');
	}

	unsigned int fieldlen = sanitized.length() - NODIGITS -digits;

	string res = sanitized.substr(NODIGITS, digits) + "." + sanitized.substr(NODIGITS+digits, fieldlen);

	return res;
};


std::set<string> MetaAccess::unsanitizeSet(std::set<string> sanitized) {
	std::set<string> unsanitized;
	std::set<string>::iterator it;
	for(it = sanitized.begin(); it != sanitized.end(); it++) {
		unsanitized.insert(unsanitize(*it));
	}
	return unsanitized;
}


void MetaAccess::addEquals(string princ1, string princ2) {
	if (VERBOSE) {
		cerr << " --> " << princ1 << " = " << princ2 << endl;
	}
	//remove any illegal characters (generally, just '.')
	princ1 = sanitize(princ1);
	princ2 = sanitize(princ2);

	string gen;
	bool princ1_in_prinToGen = (prinToGen.find(princ1) == prinToGen.end());
	bool princ2_in_prinToGen = (prinToGen.find(princ2) == prinToGen.end());

	//case 1: neither princ has a generic yet, create a new generic for the two of them
	if (princ1_in_prinToGen && princ2_in_prinToGen) {
		gen = createGeneric(princ1);
		prinToGen[princ2] = gen;
		genToPrin[gen].insert(princ2);
		return;
	}

	//case 2: only princ1 has a generic, add princ2 to that generic
	if (!princ1_in_prinToGen && princ2_in_prinToGen) {
		gen = prinToGen[princ1];
		prinToGen[princ2] = gen;
		genToPrin[gen].insert(princ2);
		return;
	}

	//case 3: only princ2 has a generic, add princ1 to that generic
	if (princ1_in_prinToGen && !princ2_in_prinToGen) {
		gen = prinToGen[princ2];
		prinToGen[princ1] = gen;
		genToPrin[gen].insert(princ1);
		return;
	}

  //case 4: both have generics, merge them into princ1's generic
  gen = prinToGen[princ1];
  string gen2 = prinToGen[princ2];
  std::set<string>::iterator it;
  //update givesPsswd
  if (givesPsswd.find(gen2) != givesPsswd.end()) {
    givesPsswd.insert(gen);
    givesPsswd.erase(gen2);
  }
  //update hasAccessTo/Accessible information, and delete gen2 from lists
  if (genHasAccessToList.find(gen2) != genHasAccessToList.end()) {
    std::set<string> gen2HasAccessTo = genHasAccessToList[gen2];
    for(it = gen2HasAccessTo.begin(); it != gen2HasAccessTo.end(); it++) {
      genHasAccessToList[gen].insert((*it));
      genAccessibleByList[(*it)].insert(gen);
      if (genAccessibleByList[(*it)].find(gen2) != genHasAccessToList[(*it)].end()) {
	genAccessibleByList[(*it)].erase(gen2);
      }
    }
    if (genHasAccessToList.find(gen2) != genHasAccessToList.end()) {
      genHasAccessToList.erase(gen2);
    }
  }
  if (genAccessibleByList.find(gen2) != genAccessibleByList.end()) {
    std::set<string> gen2Accessible = genAccessibleByList[gen2];
    for(it = gen2Accessible.begin(); it != gen2Accessible.end(); it++) {
      genAccessibleByList[gen].insert(*it);
      genHasAccessToList[(*it)].insert(gen);
      if (genHasAccessToList[(*it)].find(gen2) != genHasAccessToList[(*it)].end()) {
	genHasAccessToList[(*it)].erase(gen2);
      }
    }
    if (genAccessibleByList.find(gen2) != genAccessibleByList.end()) {
      genAccessibleByList.erase(gen2);
    }
  }
  //update equals relations
  for(it = genToPrin[gen2].begin(); it != genToPrin[gen2].end(); it++) {
    genToPrin[gen].insert(*it);
  }
  genToPrin.erase(gen2);
  prinToGen[princ2] = gen;
}


void MetaAccess::addAccess(string princHasAccess, string princAccessible) {
	if (VERBOSE) {
		cerr << " --> " << princHasAccess << " has access to "  << princAccessible << endl;
	}

	//get the generic principles these princs are part of
	string genHasAccess = getGeneric(princHasAccess);
	string genAccessible = getGeneric(princAccessible);

	std::set<string>::iterator it;

	//add to genHasAccessToList as:  genHasAccess --> [genAccessible]
	if (genHasAccessToList.find(genHasAccess) == genHasAccessToList.end()) {
		std::set<string> genAccessible_set;
		genAccessible_set.insert(genAccessible);
		genHasAccessToList[genHasAccess] = genAccessible_set;
	} else {
		genHasAccessToList[genHasAccess].insert(genAccessible);
	}

	//add to genAcccesibleBy as:  genAccessible --> [genHasAccess]
	if (genAccessibleByList.find(genAccessible) == genAccessibleByList.end()) {
		std::set<string> genHasAccess_set;
		genHasAccess_set.insert(genHasAccess);
		genAccessibleByList[genAccessible] = genHasAccess_set;
	} else {
		genAccessibleByList[genAccessible].insert(genHasAccess);
	}
}

void MetaAccess::addGives(string princ) {
	if (VERBOSE) {
		cerr << " --> " << princ << " gives psswd" << endl;
	}
	//get the generic principle princ is part of
	string gen_gives = getGeneric(princ);
	//add the generic to the set of generics that give passwords
	givesPsswd.insert(gen_gives);
}


std::set<string> MetaAccess::getTypesAccessibleFrom(string princ) {
	assert_s(prinToGen.find(sanitize(princ)) != prinToGen.end(), "input " + princ + " to getAccessibleFrom is not a known principle");
	string gen_accessed = getGeneric(princ);
	std::set<string> accessible_from;
	std::set<string>::iterator it;
	std::set<string>::iterator it2;
	//things accessible from this principle
	if(genAccessibleByList.find(gen_accessed) != genAccessibleByList.end()) {
		for(it = genAccessibleByList[gen_accessed].begin(); it != genAccessibleByList[gen_accessed].end(); it++) {
			assert_s(genToPrin.find(*it) != genToPrin.end(), "getAccessibleFrom: gen not in genToPrin");
			std::set<string> from_set = genToPrin[(*it)];
			for(it2 = from_set.begin(); it2 != from_set.end(); it2++) {
				accessible_from.insert(*it2);
			}
		}
	}
	//things equal to this principle
	assert_s(genToPrin.find(gen_accessed) != genToPrin.end(), "getAccessibleFrom: input not known");
	/*if(genToPrin[gen_accessed].size() > 1) {
		for(it = genToPrin[gen_accessed].begin(); it != genToPrin[gen_accessed].end(); it++) {
			accessible_from.insert(*it);
		}
		}*/

	return unsanitizeSet(accessible_from);
}

std::set<string> MetaAccess::getGenAccessibleFrom(string gen) {
	assert_s(genToPrin.find(gen) != genToPrin.end(), "input to getGenAccessibleFrom is not a known principal");
	string gen_accessed = gen;
	std::set<string> accessible_from;
	std::set<string>::iterator it;
	//things accessible from this principle
	accessible_from.insert(gen_accessed);
	if(genAccessibleByList.find(gen_accessed) != genAccessibleByList.end()) {
		for(it = genAccessibleByList[gen_accessed].begin(); it != genAccessibleByList[gen_accessed].end(); it++) {
			accessible_from.insert(*it);
		}
	}

	return accessible_from;
}

std::set<string> MetaAccess::getTypesHasAccessTo(string princ) {
	assert_s(prinToGen.find(sanitize(princ)) != prinToGen.end(), "input to getHasAccessTo is not a known principle");
	string gen_accessing = getGeneric(princ);
	std::set<string> can_access;
	std::set<string>::iterator it;
	std::set<string>::iterator it2;
	//things accessible from this principle
	if(genHasAccessToList.find(gen_accessing) != genHasAccessToList.end()) {
		for(it = genHasAccessToList[gen_accessing].begin(); it != genHasAccessToList[gen_accessing].end(); it++) {
			assert_s(genToPrin.find(*it) != genToPrin.end(), "getHasAccessTo: gen not in genToPrin");
			std::set<string> from_set = genToPrin[(*it)];
			for(it2 = from_set.begin(); it2 != from_set.end(); it2++) {
				can_access.insert(*it2);
			}
		}
	}
	//things equal to this principle
	assert_s(genToPrin.find(gen_accessing) != genToPrin.end(), "getHasAccessTo: input not known");
	/*if(genToPrin[gen_accessing].size() > 1) {
		for(it = genToPrin[gen_accessing].begin(); it != genToPrin[gen_accessing].end(); it++) {
			can_access.insert(*it);
		}
		}*/

	return unsanitizeSet(can_access);
}

std::set<string> MetaAccess::getGenHasAccessTo(string gen) {
	assert_s(genToPrin.find(gen) != genToPrin.end(), "input to getHasAccessTo is not a known principle");
	string gen_accessing = gen;
	std::set<string> can_access;
	std::set<string>::iterator it;
	can_access.insert(gen_accessing);
	//things accessible from this principle
	if(genHasAccessToList.find(gen_accessing) != genHasAccessToList.end()) {
		for(it = genHasAccessToList[gen_accessing].begin(); it != genHasAccessToList[gen_accessing].end(); it++) {
			can_access.insert(*it);
		}
	}

	return can_access;
}

std::set<string> MetaAccess::getEquals(string princ) {
	assert_s(prinToGen.find(sanitize(princ)) != prinToGen.end(), "input to getEquals is not a known principle");
	string gen = getGeneric(princ);
	return unsanitizeSet(genToPrin[gen]);
}

bool MetaAccess::isGives(string princ) {
	assert_s(prinToGen.find(sanitize(princ)) != prinToGen.end(), "input to isGives is not a known principle");
	string gen = getGeneric(princ);
	if (givesPsswd.find(gen) != givesPsswd.end()) {
		return true;
	}
	return false;
}

bool MetaAccess::isGenGives(string gen) {
	if (givesPsswd.find(gen) != givesPsswd.end()) {
		return true;
	}
	return false;
}

string MetaAccess::getTable(string hasAccess, string accessTo) {
  cerr << hasAccess << "->" << accessTo << endl;
	assert_s(genHasAccessToGenTable.find(hasAccess) != genHasAccessToGenTable.end(), "table for that hasAccess->accessTo does not exist");
	map<string,string> test = genHasAccessToGenTable[hasAccess];
	assert_s(genHasAccessToGenTable[hasAccess].find(accessTo) != genHasAccessToGenTable[hasAccess].end(), "requested table does not exist -- did you call MetaAccess->CreateTables()?");
	return genHasAccessToGenTable[hasAccess][accessTo];
}

string MetaAccess::getGeneric(string princ) {
	//remove any illegal characters (generally, just '.')
	princ = sanitize(princ);
	//if this principle has no generic, create one with the name gen_princ
	if (prinToGen.find(princ) == prinToGen.end()) {
		createGeneric(princ);
	}

	return prinToGen[princ];
}

string MetaAccess::getGenericPublic(string princ) {
	princ = sanitize(princ);
	if (prinToGen.find(princ) == prinToGen.end()) {
		if (VERBOSE) {
			cerr << "Could not find generic for " << princ << endl;
		}
		return "";
	}
	return prinToGen[princ];
}

string MetaAccess::createGeneric(string clean_princ) {
	string gen = "gen_" + clean_princ;
	prinToGen[clean_princ] = gen;
	std::set<string> princ_set;
	princ_set.insert(clean_princ);
	genToPrin[gen] = princ_set;
	return gen;
}

bool MetaAccess::CheckAccess() {
  std::set<string>::iterator gives;
  std::set<string> results;
  cerr << "CHECKING" << endl;

  for (gives = givesPsswd.begin(); gives != givesPsswd.end(); gives++) {
    //cerr << *gives << endl;
    std::set<string> current_layer = getGenHasAccessTo(*gives);
    std::set<string> next_layer;
    std::set<string>::iterator current_node;
    std::set<string>::iterator next_node;
    
    results.insert(*gives);
    
    for(current_node = current_layer.begin(); current_node != current_layer.end(); current_node++) {
      results.insert(*current_node);
    }
    
    while(current_layer.size() != 0) {
      for(current_node = current_layer.begin(); current_node != current_layer.end(); current_node++) {
	std::set<string> next = getGenHasAccessTo(*current_node);
	for(next_node = next.begin(); next_node != next.end(); next_node++) {
	  if (results.find(*next_node) == results.end()) {
	    results.insert(*next_node);
	    next_layer.insert(*next_node);
	  }
	}
      }
      current_layer = next_layer;
      next_layer.clear();
    }
  }

  if (results.size() != genToPrin.size()) {
    if(VERBOSE) { cerr << "wrong number of results" << endl; }
    return false;
  }

  for (gives = results.begin(); gives != results.end(); gives++) {
    if (genToPrin.find(*gives) == genToPrin.end()) {
      if (VERBOSE) { cerr << "wrong results" << endl; }
      return false;
    }
  }

	return true;
}

int MetaAccess::CreateTables() {
	assert_s(CheckAccess(), "ERROR: there is an access chain that does not terminate at a givesPsswd principle");
	string sql, num;
	map<string, std::set<string> >::iterator it;
	std::set<string>::iterator it_s;
	//Public Keys table
	sql = "DROP TABLE IF EXISTS " + public_table;
	if(!conn->execute(getCStr(sql))) {
		cerr << "error with sql query " << sql << endl;
		return -1;
	}
	sql = "CREATE TABLE " + public_table + " (Type " +  PRINCTYPE + ", Value "PRINCVALUE", Asym_Public_Key "TN_PK_KEY", Asym_Secret_Key "TN_PK_KEY", Salt bigint, PRIMARY KEY (Type,Value))";
	if(!conn->execute(getCStr(sql))) {
		cerr << "error with sql query " << sql << endl;
		return -1;
	}
	//Tables for each principle access link
	for(it = genHasAccessToList.begin(); it != genHasAccessToList.end(); it++) {
		for(it_s = it->second.begin(); it_s != it->second.end(); it_s++) {
			num = marshallVal(table_num);
			table_num++;
			sql = "DROP TABLE IF EXISTS " + table_name + num;
			if(!conn->execute(getCStr(sql))) {
			  cerr << "error with sql query " << sql << endl;
			  return -1;
			}			
			sql = "CREATE TABLE " + table_name + num + " (" + it->first + " " +  PRINCVALUE + ", " + *it_s + " " + PRINCVALUE + ", Sym_Key "TN_SYM_KEY", Salt bigint, Asym_Key "TN_PK_KEY", PRIMARY KEY (" + it->first + "," + *it_s + "))";
			if(!conn->execute(getCStr(sql))) {
				cerr << "error with sql query " << sql << endl;
				return -1;
			}
			genHasAccessToGenTable[it->first][(*it_s)] = table_name + num;
		}
	}
	return 0;
}

int MetaAccess::DeleteTables() {
	string sql, num;
	map<string, std::set<string> >::iterator it;
	std::set<string>::iterator it_s;
	//Public Keys table
	//TODO: fix PRINCVALUE to be application specific
	sql = "DROP TABLE " + public_table + ";";
	if(!conn->execute(getCStr(sql))) {
		cerr << "error with sql query " << sql << endl;
		return -1;
	}
	//Tables for each principle access link
	for(unsigned int i = 0; i < table_num; i++) {
		unsigned int n = i;
		num = marshallVal(n);
		sql = "DROP TABLE " + table_name + num + ";";
		if(!conn->execute(getCStr(sql))) {
			cerr << "error with sql query " << sql << endl;
			return -1;
		}
	}
	table_num = 0;
	return 0;
}

void MetaAccess::finish() {
	DeleteTables();
	prinToGen.clear();
	genToPrin.clear();
	genHasAccessToList.clear();
	genAccessibleByList.clear();
	givesPsswd.clear();
	genHasAccessToGenTable.clear();
}

MetaAccess::~MetaAccess() {
	finish();
}

void MetaAccess::PrintMaps() {
	map<string, string>::iterator it_m;
	map<string, std::set<string> >::iterator it_ms;
	std::set<string>::iterator it_s;

	cerr << "Principle --> Generic" << endl;
	for(it_m = prinToGen.begin(); it_m != prinToGen.end(); it_m++) {
		cerr << "  "  << it_m->first << "->" << it_m->second << endl;
	}

	cerr << "Generic --> Principle" << endl;
	for(it_ms = genToPrin.begin(); it_ms != genToPrin.end(); it_ms++) {
		cerr << "  " << it_ms->first << "->";
		for(it_s = it_ms->second.begin(); it_s != it_ms->second.end(); it_s++) {
			cerr << *it_s << " ";
		}
		cerr << endl;
	}

	cerr << "Principle ---can access---> Principle" << endl;
	for(it_ms = genHasAccessToList.begin(); it_ms != genHasAccessToList.end(); it_ms++) {
		cerr << "  " << it_ms->first << "->";
		for(it_s = it_ms->second.begin(); it_s != it_ms->second.end(); it_s++) {
			cerr << *it_s << " ";
		}
		cerr << endl;
	}

	cerr << "Principle <---can access--- Principle" << endl;
	for(it_ms = genAccessibleByList.begin(); it_ms != genAccessibleByList.end(); it_ms++) {
		cerr << "  " << it_ms->first << "->";
		for(it_s = it_ms->second.begin(); it_s != it_ms->second.end(); it_s++) {
			cerr << *it_s << " ";
		}
		cerr << endl;
	}

	cerr << "Gives Password:\n  ";
	for(it_s = givesPsswd.begin(); it_s != givesPsswd.end(); it_s++) {
		cerr << *it_s << " ";
	}
	cerr << endl;
}

//------------------------------------------------------------------------------------------


KeyAccess::KeyAccess(Connect * connect) {
	this->VERBOSE = VERBOSE_KEYACCESS;
	this->meta = new MetaAccess(connect, VERBOSE);
	this->crypt_man = new CryptoManager(randomBytes(AES_KEY_BYTES));
	this->conn = connect;
	this->meta_finished = false;
}

int KeyAccess::addEquals(string prin1, string prin2) {
	if (meta_finished) {
		cerr << "ERROR: trying to add equals after meta is finished" << endl;
		return -1;
	}

	meta->addEquals(prin1, prin2);
	return 0;
}

int KeyAccess::addAccess(string hasAccess, string accessTo) {
	if (meta_finished) {
		cerr << "ERROR: trying to add access after meta is finished" << endl;
		return -1;
	}

	meta->addAccess(hasAccess, accessTo);
	return 0;
}  

int KeyAccess::addGives(string prin) {
	if (meta_finished) {
		cerr << "ERROR: trying to add gives password after meta is finished" << endl;
		return -1;
	}

	meta->addGives(prin);
	return 0;
}

int KeyAccess::CreateTables() {
	if (meta_finished) {
		cerr << "ERROR: trying to create tables after meta is finished" << endl;
		return -1;
	}

	return meta->CreateTables();
}

int KeyAccess::DeleteTables() {
	return meta->DeleteTables();
}

std::set<string> KeyAccess::getTypesAccessibleFrom(string princ) {
	return meta->getTypesAccessibleFrom(princ);
}
std::set<string> KeyAccess::getGenAccessibleFrom(string princ) {
	return meta->getGenAccessibleFrom(princ);
}
std::set<string> KeyAccess::getTypesHasAccessTo(string princ) {
	return meta->getTypesHasAccessTo(princ);
}
std::set<string> KeyAccess::getGenHasAccessTo(string princ) {
	return meta->getGenHasAccessTo(princ);
}
std::set<string> KeyAccess::getEquals(string princ) {
	return meta->getEquals(princ);
}

string KeyAccess::getGeneric(string prin) {
	return meta->getGenericPublic(prin);
}

void KeyAccess::Print() {
  return meta->PrintMaps();
}

int KeyAccess::insert(Prin hasAccess, Prin accessTo) {
	if (VERBOSE) {
		cerr << "-->insert " << hasAccess.type << " = " << hasAccess.value << " has access to " << accessTo.type << " = " << accessTo.value << endl;
	}

	//check that we're not trying to generate a
	assert_s(!(meta->isGives(hasAccess.type) && !getKey(hasAccess) && !isInstance(hasAccess)), "cannot create a givesPsswd key");

	hasAccess.gen = meta->getGenericPublic(hasAccess.type);
	accessTo.gen = meta->getGenericPublic(accessTo.type);
	string table = meta->getTable(hasAccess.gen, accessTo.gen);
	string sql;


	//check to see if key is already in table
	std::set<Prin> prins;
	prins.insert(hasAccess);
	prins.insert(accessTo);

	if (Select(prins,table,"*")->size() > 1) {
		if (VERBOSE) {
			cerr << "relation " + hasAccess.gen + "=" + hasAccess.value + "->" + accessTo.gen + "=" + accessTo.value + " already exists" << endl;
		}
		return 1;
	}

	//Get key for this accessTo
	unsigned char * accessToKey;
	bool already_in_keys = false;

	//check to see if we already hold keys
	if (keys.find(accessTo) != keys.end()) {
		if (VERBOSE) {
			cerr << "key for " + accessTo.gen + "=" + accessTo.value + " is already held" << endl;
		}
		keys[accessTo].principles_with_access.insert(hasAccess);
		accessToKey = keys[accessTo].key;
		already_in_keys = true;
	}

	//see if there are any entries with the same second field
	prins.clear();
	prins.insert(accessTo);
	vector<vector<string> > * resultset = Select(prins,table,"*");
	vector<vector<string> >::iterator it;
	//keys for this link exist; decrypt them
	if(resultset->size() > 1) {
	        it = resultset->begin();
		it++;
		for(; it != resultset->end(); it++) {
			Prin this_row;
			this_row.type = hasAccess.type;
			this_row.gen = hasAccess.gen;
			this_row.value = it->at(0);
			unsigned char * key_for_decryption = getKey(this_row);
			if (key_for_decryption) {
				PrinKey accessToPrinKey = decryptSym(it->at(2), key_for_decryption, it->at(3));
				accessToKey = accessToPrinKey.key;
				break;
			}
		}
	}
	//keys for this link don't exist; generate them
	else if (!already_in_keys) {
		accessToKey = randomBytes(AES_KEY_BYTES);
	}

	//key exists, but we can't decrypt it?
	if(!accessToKey) {
		cerr << "ERROR: cannot decrypt this key" << endl;
		return -1;
	}

	//get sym key for hasAccess
	PrinKey hasAccessPrinKey;
	unsigned char * hasAccessKey;
	//if orphan, getPrinKey will generate key
	hasAccessPrinKey = getPrinKey(hasAccess);
	hasAccessKey = hasAccessPrinKey.key;
	unsigned char * encrypted_accessToKey;
	string string_encrypted_accessToKey;
	int encrypted_length;

	if(hasAccessKey) {
		uint64_t salt = randomValue();
		AES_KEY * aes = crypt_man->get_key_SEM(hasAccessKey);
		encrypted_accessToKey = crypt_man->encrypt_SEM(accessToKey, AES_KEY_BYTES, aes, salt);
		string string_salt = marshallVal(salt);
		string_encrypted_accessToKey = marshallBinary(encrypted_accessToKey,AES_KEY_BYTES);
		sql = "INSERT INTO " + table + "(" + hasAccess.gen + ", " + accessTo.gen + ", Sym_Key, Salt) VALUES ('" + hasAccess.value + "', '" + accessTo.value + "', " + string_encrypted_accessToKey + ", " + string_salt + ");";
	}

	//couldn't get symmetric key for hasAccess, so get public key
	else {
		PKCS * hasAccess_publicKey = getPublicKey(hasAccess);
		assert_s(hasAccess_publicKey, "Could not access public key");
		encrypted_accessToKey = crypt_man->encrypt(hasAccess_publicKey,accessToKey,AES_KEY_BYTES,encrypted_length);
		assert_s(encrypted_accessToKey, "Could not encrypt accessToKey");
		string_encrypted_accessToKey = marshallBinary(encrypted_accessToKey,encrypted_length);
		sql = "INSERT INTO " + table + "(" + hasAccess.gen + ", " + accessTo.gen + ", Asym_Key) VALUES ('" + hasAccess.value + "', '" + accessTo.value + "', " + string_encrypted_accessToKey + ");";
	}

	//update table with encrypted key
	if(!conn->execute(getCStr(sql))) {
		cerr << "Problem with sql statement: " << sql << endl;
		return -1;
	}

	//store key locally if either user is logged on
	PrinKey accessToPrinKey = buildKey(hasAccess, accessToKey, AES_KEY_BYTES);
	accessToPrinKey.principles_with_access.insert(accessTo);
	if (!already_in_keys && (getKey(hasAccess) || getKey(accessTo))) {
		addToKeys(accessTo, accessToPrinKey);
	}

	//check that accessTo has publics key; if not generate them
	if (!isInstance(accessTo)) {
		sql = "SELECT * FROM " + meta->publicTableName() + " WHERE Type='" + accessTo.gen + "' AND Value='" + accessTo.value + "';";
		DBResult * dbres;
		if (!conn->execute(getCStr(sql),dbres)) {
			cerr << "Problem with sql statement: " << sql << endl;
		}
		vector<vector<string> > * res = conn->unpack(dbres);
		if (res->size() < 2) {
			GenerateAsymKeys(accessTo,accessToPrinKey);
		}
	}



	//orphans
	std::set<Prin> hasAccess_set;
	hasAccess_set.insert(hasAccess);
	std::set<Prin> accessTo_set;
	accessTo_set.insert(accessTo);
	bool hasAccess_has_children = (orphanToChildren.find(hasAccess) != orphanToChildren.end());
	bool accessTo_has_parents = (orphanToParents.find(accessTo) != orphanToParents.end());

	if (!isInstance(hasAccess)) {
		//add to orphan graphs
		orphanToParents[accessTo] = hasAccess_set;
		orphanToChildren[hasAccess] = accessTo_set;
		//set up asymmetric encryption
		sql = "SELECT * FROM " + meta->publicTableName() + " WHERE Type='" + hasAccess.gen + "' AND Value='" + hasAccess.value + "';";
		DBResult * dbres;
		if (!conn->execute(getCStr(sql),dbres)) {
			cerr << "Problem with sql statement: " << sql << endl;
		}
		vector<vector<string> > * res = conn->unpack(dbres);
		assert_s(hasAccessPrinKey.key, "created hasAccess has no key");
		if (res->size() < 2) {
			GenerateAsymKeys(hasAccess,hasAccessPrinKey);
		}
		addToKeys(accessTo, accessToPrinKey);
		addToKeys(hasAccess, hasAccessPrinKey);
		assert_s(isOrphan(hasAccess), "orphan principle is not checking as orphan");
		assert_s(isInstance(hasAccess), "orphan hasAccess thinks it doesn't exist >_<");
		return 0;
	}

	if (isOrphan(hasAccess)) {
		if (hasAccess_has_children) {
			orphanToChildren[hasAccess].insert(accessTo);
		} else {
			orphanToChildren[hasAccess] = accessTo_set;
		}
		if (accessTo_has_parents) {
			orphanToParents[accessTo].insert(hasAccess);
		} else {
			orphanToParents[accessTo] = hasAccess_set;
		}
		if (!already_in_keys) {
			addToKeys(accessTo, accessToPrinKey);
		}
		return 0;
	}

	if (isOrphan(accessTo) && !isOrphan(hasAccess)) {
		return removeFromOrphans(accessTo);
	}

	assert_s(isInstance(hasAccess), "hasAccess does not exist; this is an orphan");

	return 0;
}

int KeyAccess::remove(Prin hasAccess, Prin accessTo) {
	if(VERBOSE) {
		cerr << "-->remove " << hasAccess.type << "=" << hasAccess.value << "->" << accessTo.type << "=" << accessTo.value << endl;
	}

	if(hasAccess.gen == "") {
		hasAccess.gen = meta->getGenericPublic(hasAccess.type);
	}
	if(accessTo.gen == "") {
		accessTo.gen = meta->getGenericPublic(accessTo.type);
	}

	assert_s(isInstance(hasAccess), "hasAccess in remove is has not been inserted");
	assert_s(isInstance(accessTo), "accessTo in remove is has not been inserted");

	//remove hasAccess from accessTo's principles_with_access if local key is stored
	if (getKey(accessTo)) {
		PrinKey accessTo_key = keys[accessTo];
		accessTo_key.principles_with_access.erase(hasAccess);
		keys[accessTo] = accessTo_key;
		//if this was the only link keeping accessTo's key accessible, delete the entire subtree
		if (accessTo_key.principles_with_access.size() <= 1) {
			return removePsswd(accessTo);
		}
	}

	//remove hasAccess from accessTo's table
	return RemoveRow(hasAccess, accessTo);

	return 0;
}


int KeyAccess::removeFromOrphans(Prin orphan) {
	//remove descendants from orphanToChildren
	if(VERBOSE) {
		cerr << "   removing " << orphan.gen << "=" << orphan.value << " from orphans" << endl;
	}
	list<Prin> children;
	std::set<Prin>::iterator it;
	children.push_back(orphan);
	for(it = orphanToChildren[orphan].begin(); it != orphanToChildren[orphan].end(); it++) {
		children.push_back(*it);
	}
	list<Prin>::iterator child = children.begin();
	map<Prin, std::set<Prin> >::iterator it_map = orphanToParents.begin();
	while (child != children.end()) {
		if (orphanToChildren.find(*child) != orphanToChildren.end()) {
			for (it = orphanToChildren[*child].begin(); it != orphanToChildren[*child].end(); it++) {
				children.push_back(*it);
			}
		}
		child++;
	}
	for (child = children.begin(); child != children.end(); child++) {
		if (orphanToChildren.find(*child) != orphanToChildren.end()) {
			orphanToChildren.erase(*child);
		}
		if (orphanToParents.find(*child) != orphanToParents.end()) {
			orphanToParents.erase(*child);
		}
	}

	//remove descendants from orphanToParents
	while (it_map != orphanToParents.end()) {
		if (inList(children, it_map->first)) {
			orphanToParents.erase(it_map->first);
		}
		it_map++;
	}

	//check to see if orphans' keys should be accessible
	std::set<Prin>::iterator princ_access;
	for (princ_access = keys[orphan].principles_with_access.begin(); princ_access != keys[orphan].principles_with_access.end(); princ_access++) {
		if (keys.find(*princ_access) != keys.end() && *princ_access != orphan) {
			return 0;
		}
	}
	for (child = children.begin(); child != children.end(); child++) {
		keys.erase(*child);
	}
	keys.erase(orphan);
	return 0;
}

unsigned char * KeyAccess::getKey(Prin prin) {
	if(prin.gen == "") {
		prin.gen = meta->getGenericPublic(prin.type);
	}
	PrinKey prinkey = getPrinKey(prin);
	if(VERBOSE) {
		cerr << "     " << prin.gen  << "=" << prin.value << " has principles with access: " << endl;
		std::set<Prin>::iterator it;
		for(it = prinkey.principles_with_access.begin(); it != prinkey.principles_with_access.end(); it++) {
			cerr << "\t" << it->gen << "=" << it->value << endl;
		}
	}
	cerr << "returning null? " << (prinkey.key == NULL) << "\n";
	return prinkey.key;
}

PrinKey KeyAccess::getPrinKey(Prin prin) {
	if(prin.gen == "") {
		prin.gen = meta->getGenericPublic(prin.type);
	}

	if(VERBOSE) {
		cerr << "   fetching key for " << prin.gen << " " << prin.value << endl;
	}

	if(keys.find(prin) != keys.end()) {
		assert_s(keys[prin].len == AES_KEY_BYTES, "getKey is trying to return a key of the wrong length");
		return keys[prin];
	}

	PrinKey prinkey = getUncached(prin);
	if (prinkey.len != 0) {
	  return prinkey;
	}

	//is orphan
	if (!isInstance(prin)) {
	  unsigned char * key = randomBytes(AES_KEY_BYTES);
	  prinkey = buildKey(prin, key, AES_KEY_BYTES);
	  addToKeys(prin, prinkey);
	  GenerateAsymKeys(prin, prinkey);
	  std::set<Prin> empty_set;
	  orphanToParents[prin] = empty_set;
	  orphanToChildren[prin] = empty_set;
	  assert_s(isInstance(prin), "newly created orphan in getKey is not recognized as an instance");
	  assert_s(isOrphan(prin), "newly created orphan in getKey is not recognized as an orphan");
	}
	else {
	  if (VERBOSE) {
	    cerr << "     *asking for a key that exists but is not accessible" << endl;
	  }
	}
	return prinkey;
}

PKCS * KeyAccess::getPublicKey(Prin prin) {
	if(VERBOSE) {
		cerr << "   getting public key for " << prin.gen << " " << prin.value << endl;
	}
	assert_s(isInstance(prin), "prin input to getPublicKey has never been seen before");
	assert_s(prin.gen != "", "input into getPublicKey has an undefined generic");
	string sql = "SELECT * FROM " + meta->publicTableName() + " WHERE Type='" + prin.gen + "' AND Value='" + prin.value + "'";
	DBResult* dbres;
	if (!conn->execute(getCStr(sql), dbres)) {
		cerr << "SQL error from query: " << sql << endl;
		return NULL;
	}
	vector< vector<string> > * res = conn->unpack(dbres);
	if(res->size() < 2) {
		cerr << "No public key for input to getPublicKey" << endl;
		return NULL;
	}
	unsigned int length;
	unsigned char * key = unmarshallBinary(getCStr(res->at(1).at(2)),(res->at(1).at(2)).length(), length);
	return crypt_man->unmarshallKey(key,length,true);
}

PrinKey KeyAccess::getSecretKey(Prin prin) {
	if(VERBOSE) {
		cerr << "   fetching secret key" << endl;
	}
	assert_s(isInstance(prin), "prin input to getSecretKey has never been seen before");
	assert_s(prin.gen != "", "input into getSecretKey has an undefined generic");
	string sql = "SELECT * FROM " + meta->publicTableName() + " WHERE Type='" + prin.gen + "' AND Value='" + prin.value + "'";
	DBResult* dbres;
	PrinKey error;
	error.len = 0;
	error.key = NULL;
	if (!conn->execute(getCStr(sql), dbres)) {
		cerr << "SQL error from query: " << sql << endl;
		return error;
	}
	vector< vector<string> > * res = conn->unpack(dbres);
	if(res->size() < 2) {
		cerr << "No public key for input to getSecretKey" << endl;
		return error;
	}
	string string_key = res->at(1).at(3);
	return decryptSym(res->at(1).at(3), getKey(prin), res->at(1).at(4));
}

int KeyAccess::insertPsswd(Prin gives, unsigned char * psswd) {
	if(VERBOSE) {
		cerr << "-->" << gives.type << " " << gives.value << " is logging in with ";
		myPrint(psswd,AES_KEY_BYTES);
		cerr << endl;
	}

	int ret = 0;

	gives.gen = meta->getGenericPublic(gives.type);
	std::set<string> gives_hasAccessTo = meta->getGenHasAccessTo(gives.gen);

	//put password into local keys
	PrinKey password = buildKey(gives, psswd, AES_KEY_BYTES);
	addToKeys(gives, password);

	//check if this person has a asym key (that is, if gives is an instance that has been inserted before)
	if (!isInstance(gives)) {
		GenerateAsymKeys(gives, password);
		return 0;
	}

	//get a list of all possible gens gives could access
	list<string> gives_hasAccess = DFS_hasAccess(gives);

	//sort through the list, getting keys if they exist, and decrypting and storing them
	list<string>::iterator accessTo = gives_hasAccess.begin();
	list<string>::iterator hasAccess = gives_hasAccess.begin();
	accessTo++;
	std::set<Prin> accessible_values;
	std::set<Prin> values;
	accessible_values.insert(gives);
	for (hasAccess = gives_hasAccess.begin(); hasAccess != gives_hasAccess.end(); hasAccess++) {
		for (accessTo = gives_hasAccess.begin(); accessTo != gives_hasAccess.end(); accessTo++) {
			std::set<string> acc_to = meta->getGenHasAccessTo(*hasAccess);
			std::set<string>::iterator i;
			if (acc_to.find(*accessTo) == acc_to.end() || accessTo == hasAccess) {
				continue;
			}
			Prin hasAccess_prin;
			std::set<Prin>::iterator v;
			for (v = accessible_values.begin(); v != accessible_values.end(); v++) {
				if (hasAccess->compare(v->gen) == 0) {
					values.insert(*v);
				}
			}
			for (v = values.begin(); v != values.end(); v++) {
				hasAccess_prin = *v;
				std::set<Prin> prin_set;
				prin_set.insert(hasAccess_prin);
				assert_s(hasAccess->compare(hasAccess_prin.gen) == 0, "hasAccess_prin in insertPsswd is WRONG");
				int number_keys = SelectCount(prin_set, meta->getTable(*hasAccess,*accessTo));
				//if there are many keys of this type, don't store them in local memory
				if (number_keys > THRESHOLD) {
					if (VERBOSE) {
						cerr << "caching " << number_keys << " for " << *accessTo << endl;
					}
					if (uncached_keys.find(*accessTo) != uncached_keys.end()) {
						uncached_keys[*accessTo].insert(hasAccess_prin);
					} else {
						uncached_keys[*accessTo] = prin_set;
					}
					continue;
				}
				vector<vector<string> > * res = Select(prin_set, meta->getTable(*hasAccess,*accessTo), "*");
				if (res->size() > 1) {
					vector<vector<string> >::iterator row = res->begin();
					//first row is field names
					row++;
					while (row != res->end()) {
						//remember to check this Prin on the next level
						Prin new_prin;
						new_prin.gen = res->at(0).at(1);
						new_prin.value = row->at(1);
						accessible_values.insert(new_prin);
						unsigned char * new_key = getKey(new_prin);
						PrinKey new_prin_key;
						//if key is not currently held by anyone
						if (!new_key) {
							assert_s(getKey(hasAccess_prin),"there is a logical issue with insertPsswd: getKey should have the key for hasAccess");
							if (row->at(4) == "X''") { //symmetric key okay
								new_prin_key = decryptSym(row->at(2), getKey(hasAccess_prin), row->at(3));
							} else { //use asymmetric
								PrinKey sec_key = getSecretKey(hasAccess_prin);
								new_prin_key = decryptAsym(row->at(4), sec_key.key, sec_key.len);
							}
						}
						//if key is currently held by someone else...
						else {
							new_prin_key = buildKey(new_prin, new_key, AES_KEY_BYTES);
						}
						new_prin_key.principles_with_access.insert(new_prin);
						new_prin_key.principles_with_access.insert(hasAccess_prin);
						if (addToKeys(new_prin, new_prin_key) < 0) {
							ret--;
						}
						row++;
					}
				}
			}
			values.clear();
		}
	}

	return ret;
}

int KeyAccess::removePsswd(Prin prin) {
	if(VERBOSE) {
		cerr << "-->" << prin.type << " " << prin.value << " is logging out" << endl;
	}

	if(prin.gen == "") {
		prin.gen = meta->getGenericPublic(prin.type);
	}

	assert_s(isInstance(prin), "prin in removePsswd is has not been inserted");

	list<string> hasAccessTo = BFS_hasAccess(prin);
	list<string>::iterator hasAccessTo_gen;
	map<Prin, PrinKey>::iterator key_it;
	std::set<Prin> remove_set;
	remove_set.insert(prin);
	std::set<Prin>::iterator set_it;
	map<string, std::set<Prin> >::iterator map_it;

	for (hasAccessTo_gen = hasAccessTo.begin(); hasAccessTo_gen != hasAccessTo.end(); hasAccessTo_gen++) {
		for (key_it = keys.begin(); key_it != keys.end(); key_it++) {
			if (key_it->first.gen == *hasAccessTo_gen) {
				for (set_it = remove_set.begin(); set_it != remove_set.end(); set_it++) {
					if (key_it->second.principles_with_access.find(*set_it) != key_it->second.principles_with_access.end()) {
						key_it->second.principles_with_access.erase(*set_it);
						if (key_it->second.principles_with_access.size() <= 1) {
							remove_set.insert(key_it->first);
						}
					}
				}
			}
		}
	}

	for(set_it = remove_set.begin(); set_it != remove_set.end(); set_it++) {
		for(map_it = uncached_keys.begin(); map_it != uncached_keys.end(); map_it++) {
			map_it->second.erase(*set_it);
		}
		removeFromKeys(*set_it);
	}

	return 0;
}

PrinKey KeyAccess::buildKey(Prin hasAccess, unsigned char * sym_key, int length_sym) {
	PrinKey new_key;
	new_key.key = sym_key;
	new_key.len = length_sym;
	std::set<Prin> prinHasAccess;
	prinHasAccess.insert(hasAccess);
	new_key.principles_with_access = prinHasAccess;
	return new_key;
}

int KeyAccess::addToKeys(Prin prin, PrinKey key) {
	if (prin.gen == "") {
		prin.gen = meta->getGenericPublic(prin.type);
	}
	assert_s(key.principles_with_access.find(prin) != key.principles_with_access.end(), "addToKeys hasAccess prin is not in key's principles_with_access");
	if(VERBOSE) {
		cerr << "   adding key ";
		myPrint(key.key, key.len);
		cerr << " for " << prin.gen << " " << prin.value << endl;
	}

	if (keys.find(prin) != keys.end()) {
		if (keys[prin] == key) {
			std::set<Prin>::iterator it;
			for(it = key.principles_with_access.begin(); it != key.principles_with_access.end(); it++) {
				keys[prin].principles_with_access.insert(*it);
			}
			return 1;
		}
		else {
			cerr << "prin input to addToKeys already has a different key" << endl;
			return -1;
		}
	}

	keys[prin] = key;


	std::set<Prin>::iterator set_it;
	map<Prin, PrinKey>::iterator key_it;
	int count = 0;
	int users = 0;
	for (key_it = keys.begin(); key_it != keys.end(); key_it++) {
		for (set_it = key.principles_with_access.begin(); set_it != key.principles_with_access.end(); set_it++) {
			if (key_it->first.gen.compare(set_it->gen) == 0) {
				count++;
			}
		}
		if (meta->isGenGives(key_it->first.gen)) {
			users++;
		}
	}
	if (users > 0) {
		count /= users;
	}
	if (count > THRESHOLD) {
		cerr << "WARNING: more than " << THRESHOLD << " keys on average per user of the same type have been added to the local map" << endl;
	}

	return 0;
}

int KeyAccess::removeFromKeys(Prin prin) {
	if(prin.gen == "") {
		prin.gen = meta->getGenericPublic(prin.type);
	}

	if(VERBOSE) {
		cerr << "   removing key for " << prin.gen << " " << prin.value << endl;
	}

	//remove all this prin's uncached key references
	if (uncached_keys.find(prin.gen) != uncached_keys.end()) {
		std::set<Prin> prinHasAccess = uncached_keys.find(prin.gen)->second;
		std::set<Prin>::iterator set_it;
		for (set_it = prinHasAccess.begin(); set_it != prinHasAccess.end(); set_it++) {
			std::set<Prin> prin_set;
			prin_set.insert(*set_it);
			prin_set.insert(prin);
			int count = SelectCount(prin_set, meta->getTable(set_it->gen, prin.gen));
			if (count > 0) {
				uncached_keys[prin.gen].erase(*set_it);
			}
		}
		if (uncached_keys[prin.gen].empty()) {
			uncached_keys.erase(prin.gen);
		}
	}

	if (keys.find(prin) == keys.end()) {
		if(VERBOSE) {
			cerr << "prin input to removeFromKeys does not have key" << endl;
		}
		return 1;
	}

	memset(keys[prin].key, '0', keys[prin].len);
	keys.erase(prin);
	return 0;
}

list<string> KeyAccess::BFS_hasAccess(Prin start) {
	if(start.gen == "") {
		start.gen = meta->getGenericPublic(start.type);
	}

	list<string> results;
	std::set<string> current_layer = meta->getGenHasAccessTo(start.gen);
	std::set<string> next_layer;
	std::set<string>::iterator current_node;
	std::set<string>::iterator next_node;

	results.push_back(start.gen);

	for(current_node = current_layer.begin(); current_node != current_layer.end(); current_node++) {
		if (!inList(results,*current_node)) {
			results.push_back(*current_node);
		}
	}

	while(current_layer.size() != 0) {
		for(current_node = current_layer.begin(); current_node != current_layer.end(); current_node++) {
			std::set<string> next = meta->getGenHasAccessTo(*current_node);
			for(next_node = next.begin(); next_node != next.end(); next_node++) {
				if (!inList(results,*next_node)) {
					results.push_back(*next_node);
					next_layer.insert(*next_node);
				}
			}
		}
		current_layer = next_layer;
		next_layer.clear();
	}

	return results;
}

list<string> KeyAccess::DFS_hasAccess(Prin start) {
	if(start.gen == "") {
		start.gen = meta->getGenericPublic(start.type);
	}

	list<string> results;
	std::set<string> reachable_from_current = meta->getGenHasAccessTo(start.gen);
	list<string> to_investigate;
	string current_node;
	std::set<string>::iterator it;

	results.push_back(start.gen);

	for(it = reachable_from_current.begin(); it != reachable_from_current.end(); it++) {
		if(!inList(results,*it)) {
			to_investigate.push_back(*it);
		}
	}

	while(to_investigate.size() > 0) {
		current_node = to_investigate.front();
		to_investigate.pop_front();
		while(inList(results, current_node)) {
			current_node = to_investigate.front();
			to_investigate.pop_front();
		}
		results.push_back(current_node);
		reachable_from_current = meta->getGenHasAccessTo(current_node);
		for(it = reachable_from_current.begin(); it != reachable_from_current.end(); it++) {
			if (!inList(results, *it)) {
				to_investigate.push_front(*it);
			}
		}
	}

	return results;
}

vector<vector<string> > * KeyAccess::Select(std::set<Prin> & prin_set, string table_name, string column) {
	std::set<Prin>::iterator prin;
	for(prin = prin_set.begin(); prin != prin_set.end(); prin++) {
		assert_s(prin->gen != "", "input to Select has no gen");
	}
	string sql = "SELECT " + column + " FROM " + table_name + " WHERE ";
	for(prin = prin_set.begin(); prin != prin_set.end(); prin++) {
		if(prin != prin_set.begin()) {
			sql += " AND ";
		}
		sql += prin->gen + "='" + prin->value + "'";
	}
	sql += ";";
	DBResult * dbres;
	if(!conn->execute(getCStr(sql),dbres)) {
		cerr << "SQL error with query: " << sql << endl;
		return NULL;
	}
	return conn->unpack(dbres);
}  

int KeyAccess::SelectCount(std::set<Prin> & prin_set, string table_name) {
	std::set<Prin>::iterator prin;
	for(prin = prin_set.begin(); prin != prin_set.end(); prin++) {
		assert_s(prin->gen != "", "input to Select has no gen");
	}
	string sql = "SELECT COUNT(*) FROM " + table_name + " WHERE ";
	for(prin = prin_set.begin(); prin != prin_set.end(); prin++) {
		if(prin != prin_set.begin()) {
			sql += " AND ";
		}
		sql += prin->gen + "='" + prin->value + "'";
	}
	sql += ";";
	DBResult * dbres;
	if(!conn->execute(getCStr(sql),dbres)) {
		cerr << "SQL error with query: " << sql << endl;
		return -1;
	}
	vector<vector<string> > * res = conn->unpack(dbres);
	int size = unmarshallVal(res->at(1).at(0));
	return size;
}

int KeyAccess::RemoveRow(Prin hasAccess, Prin accessTo) {
	assert_s(hasAccess.gen != "", "hasAccess input to RemoveRow has no gen");
	assert_s(accessTo.gen != "", "accessTo input to RemoveRow has no gen");
	string table = meta->getTable(hasAccess.gen, accessTo.gen);
	string sql = "DELETE FROM " + table + " WHERE " + hasAccess.gen + "='" + hasAccess.value + "' AND " + accessTo.gen + "='" + accessTo.value + "';";
	if(!conn->execute(getCStr(sql))) {
		cerr << "SQL error with query: " << sql << endl;
		return -1;
	}
	return 0;
}

int KeyAccess::GenerateAsymKeys(Prin prin, PrinKey prin_key) {
	meta_finished = true;
	string pub_key_string = "NULL";
	string encrypted_sec_key_string = "NULL";
	string salt_string = "NULL";
	if (meta->getGenHasAccessTo(prin.gen).size() > 1) {
	  AES_KEY * aes = crypt_man->get_key_SEM(prin_key.key);
	  uint64_t salt = randomValue();
	  PKCS * rsa_pub_key;
	  PKCS * rsa_sec_key;
	  crypt_man->generateKeys(rsa_pub_key,rsa_sec_key);
	  int length_pub;
	  unsigned char * pub_key = crypt_man->marshallKey(rsa_pub_key,true,length_pub);
	  int length_sec;
	  unsigned char * sec_key = crypt_man->marshallKey(rsa_sec_key,false,length_sec);
	  unsigned char * encrypted_sec_key = crypt_man->encrypt_SEM(sec_key, length_sec, aes, salt);
	  salt_string = marshallVal(salt);
	  encrypted_sec_key_string = marshallBinary(encrypted_sec_key,length_sec);
	  pub_key_string = marshallBinary(pub_key,length_pub);
	}
	string sql = "INSERT INTO " + meta->publicTableName() + " VALUES ('" + prin.gen + "', '" + prin.value + "', " + pub_key_string + ", " + encrypted_sec_key_string + ", " + salt_string + ");";
	if (!conn->execute(getCStr(sql))) {
		cerr << "SQL error on query " << sql << endl;
		return -1;
	}
	return 0;
}

PrinKey KeyAccess::decryptSym(string str_encrypted_key, unsigned char * key_for_decrypting, string str_salt) {
	if(VERBOSE) {
		cerr << "\tuse symmetric decryption" << endl;
	}
	unsigned int length;
	unsigned char * encrypted_key = unmarshallBinary(getCStr(str_encrypted_key), str_encrypted_key.length(),length);
	uint64_t salt = unmarshallVal(str_salt);
	AES_KEY * aes = crypt_man->get_key_SEM(key_for_decrypting);
	unsigned char * key = crypt_man->decrypt_SEM(encrypted_key,length,aes,salt);
	PrinKey result;
	result.len = length;
	result.key = key;
	return result;
}

PrinKey KeyAccess::decryptAsym(string str_encrypted_key, unsigned char * secret_key, int secret_key_len) {
	if(VERBOSE) {
		cerr << "\tuse asymmetric decryption" << endl;
	}
	unsigned int length;
	PKCS * pk_sec_key = crypt_man->unmarshallKey(secret_key,secret_key_len,false);
	unsigned char * encrypted_key = unmarshallBinary(getCStr(str_encrypted_key),str_encrypted_key.length(),length);
	unsigned char * key = crypt_man->decrypt(pk_sec_key, encrypted_key, length, secret_key_len);
	assert_s(secret_key_len == (unsigned int) AES_KEY_BYTES, "Secret key is the wrong length!");
	PrinKey result;
	result.len = secret_key_len;
	result.key = key;
	return result;
}

bool KeyAccess::isInstance(Prin prin) {
  if (prin.gen == "") {
    prin.gen = meta->getGenericPublic(prin.type);
  }

  Prin type;
  type.type = "Type";
  type.gen = "Type";
  type.value = prin.gen;
  Prin value;
  value.type = "Value";
  value.gen = "Value";
  value.value = prin.value;
  std::set<Prin> prin_set;
  prin_set.insert(type);
  prin_set.insert(value);

  int count = SelectCount(prin_set, meta->publicTableName());
  if (count > 0) {
    return true;
  } else {
    return false;
  }
}

bool KeyAccess::isType(string type) {
  return (meta->getGenericPublic(type).length() != 0);
}

bool KeyAccess::isOrphan(Prin prin) {
	return ((orphanToParents.find(prin) != orphanToParents.end()) || (orphanToChildren.find(prin) != orphanToChildren.end()));
}

PrinKey KeyAccess::getUncached(Prin prin) {
	if (prin.gen == "") {
		prin.gen = meta->getGenericPublic(prin.type);
	}

	if (VERBOSE) {
		cerr << "   checking for uncached keys" << endl;
	}

	PrinKey empty;
	empty.key = NULL;
	empty.len = 0;

	if (uncached_keys.find(prin.gen) == uncached_keys.end()) {
		return empty;
	}

	//key could still be in db
	std::set<Prin> prinHasAccess = uncached_keys.find(prin.gen)->second;
	std::set<Prin>::iterator set_it;
	for (set_it = prinHasAccess.begin(); set_it != prinHasAccess.end(); set_it++) {
		std::set<Prin> prin_set;
		prin_set.insert(*set_it);
		prin_set.insert(prin);
		vector<vector<string> > * res = Select(prin_set, meta->getTable(set_it->gen, prin.gen), "*");
		if (res->size() > 1) {
			PrinKey new_prin_key;
			if (res->at(1).at(4) == "X''") { //symmetric key okay
				new_prin_key = decryptSym(res->at(1).at(2), getKey(*set_it), res->at(1).at(3));
			} else { //use asymmetric
				PrinKey sec_key = getSecretKey(*set_it);
				new_prin_key = decryptAsym(res->at(1).at(4), sec_key.key, sec_key.len);
			}
			return new_prin_key;
		}
	}
	return empty;
}

void KeyAccess::finish() {
	map<Prin, PrinKey>::iterator it;
	for(it = keys.begin(); it != keys.end(); it++) {
		memset(it->second.key, '0', it->second.len);
	}
	keys.clear();
	meta->~MetaAccess();
}

KeyAccess::~KeyAccess() {
	finish();
}