#include <main/Analysis.hh>

using namespace std;


EncSet::EncSet() : osl(FULL_EncSet.osl) {}

EncSet::EncSet(FieldMeta * fm) {
    osl.clear();
    for (auto pair : fm->encdesc.olm) {
	osl[pair.first] = LevelFieldPair(pair.second, fm);
    }
}

EncSet::EncSet(const OLK & olk) {
    osl[olk.o] = LevelFieldPair(olk.l, olk.key);
}

EncSet
EncSet::intersect(const EncSet & es2) const
{
    OnionLevelFieldMap m;
    for (auto it2 = es2.osl.begin();
         it2 != es2.osl.end(); ++it2) {
        auto it = osl.find(it2->first);
        if (it != osl.end()) {
            SECLEVEL sl = (SECLEVEL)min((int)it->second.first,
                                        (int)it2->second.first);
            if (it->second.second == NULL) {
                m[it->first] = LevelFieldPair(
                        sl, it2->second.second);
            } else if (it2->second.second == NULL) {
                m[it->first] = LevelFieldPair(
                        sl, it->second.second);
            } else if (it->second.second == it2->second.second) {
                m[it->first] = LevelFieldPair(
                        sl, it->second.second);
            }
	    // different fields for same onion, level gives null intersection
        }
    }
    return EncSet(m);
}

ostream&
operator<<(ostream &out, const EncSet & es)
{
    if (es.osl.size() == 0) {
        out << "empty encset";
    }
    for (auto it : es.osl) {
        out << "(onion " << it.first
            << ", level " << levelnames[(int)it.second.first]
            << ", field `" << (it.second.second == NULL ? "*" : it.second.second->fname) << "`"
            << ") ";
    }
    return out;
}


EncDesc
EncSet::encdesc() {
    OnionLevelMap olm = OnionLevelMap();
    for (auto it : osl) {
	olm[it.first] = it.second.first;
    }
    return EncDesc(olm);
}

void
EncSet::setFieldForOnion(onion o, FieldMeta * fm) {
    LevelFieldPair lfp = getAssert(osl, o);

    osl[o] = LevelFieldPair(lfp.first, fm);
}

OLK
EncSet::chooseOne() const
{
    // Order of selection is encoded in this array.
    // The onions appearing earlier are the more preferred ones.
    static const onion onion_order[] = {
        oDET,
        oOPE,
        oAGG,
        oSWP,
	oPLAIN, 
    };

    cerr << "choosing one from " << *this << "\n";
    
    static size_t onion_size = sizeof(onion_order) / sizeof(onion_order[0]);
    for (size_t i = 0; i < onion_size; i++) {
	onion o = onion_order[i];
        auto it = osl.find(o);
        if (it != osl.end()) {
	    cerr << "chosen " << OLK(o, it->second.first, it->second.second) << "\n";
	    return OLK(o,  it->second.first, it->second.second);
        }
    }
    return OLK();
}

bool
EncSet::contains(const OLK & olk) const {
    auto it = osl.find(olk.o);
    if (it == osl.end()) {
	return false;
    }
    if (it->second.first == olk.l) {
	return true;
    }
    return false;
}


bool
needsSalt(EncSet es) {
    for (auto pair : es.osl) {
	if (pair.second.first == SECLEVEL::RND) {
	    return true;
	}
    }

    return false;
}


ostream&
operator<<(ostream &out, const reason &r)
{
    out << r.why_t_item << " PRODUCES encset " << r.encset << "\n" \
	<< " BECAUSE " << r.why_t << "\n";
    
    if (r.childr->size()) {
	out << " AND CHILDREN: {" << "\n";
	for (reason ch : *r.childr) {
	    out << ch;
	}
	out << "} \n";
    }
    return out;
}

RewritePlan::RewritePlan(const OLK & parent_olk, const OLK & childr_olk,
			 Item ** childr, uint no_childr, reason r) {
    map<Item *, OLK> childr_plan = map<Item *, OLK>();
    for (uint i = 0; i < no_childr; i++) {
	childr_plan[childr[i]] = childr_olk;
    }
    plan[parent_olk] = childr_plan;
    es_out = EncSet(parent_olk);
    this->r = r;
}


RewritePlan::RewritePlan(const EncSet & es, reason r) {
    es_out = es;
    this->r = r;
}

void
RewritePlan::restrict(const EncSet & es) {
    es_out = es_out.intersect(es);
    assert_s(!es_out.empty(), "after restrict rewrite plan has empty encset");

    if (plan.size()) { //node has children
	for (auto pair = plan.begin(); pair != plan.end(); pair++) {
	    if (!es.contains(pair->first)) {
		plan.erase(pair);
	    }
	}
    }
}


ostream&
operator<<(ostream &out, const RewritePlan * rp)
{
    if (!rp) {
        out << "NULL RewritePlan";
	return out;
    }

    out << " RewritePlan: \n---> out encset " << rp->es_out << "\n---> reason " << rp->r << "\n---> plan " << "(";
    for (auto pair: rp->plan) {
	out << "OLK " << pair.first << " ";
	for (auto pair2 : pair.second) {
	    out << " Item " << *pair2.first << " " << " OLK " << pair2.second;
	}
	cerr << "\n";
    }

    out << ")";

    return out;
}
