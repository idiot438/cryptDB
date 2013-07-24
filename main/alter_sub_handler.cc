#include <main/alter_sub_handler.hh>
#include <main/List_helpers.hh>
#include <main/rewrite_util.hh>
#include <main/enum_text.hh>
#include <parser/lex_util.hh>
#include <main/dispatcher.hh>

class AddColumnSubHandler : public AlterSubHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        const std::string &table =
            lex->select_lex.table_list.first->table_name;
        LEX *new_lex = copy(lex);

        TableMeta *tm = a.getTableMeta(table);
        // -----------------------------
        //         Rewrite TABLE
        // -----------------------------
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a);

        // Create *Meta objects.
        auto add_it =
            List_iterator<Create_field>(lex->alter_info.create_list);
        new_lex->alter_info.create_list = 
            reduceList<Create_field>(add_it, List<Create_field>(),
                [&a, &ps, &tm] (List<Create_field> out_list,
                                Create_field *cf) {
                    return createAndRewriteField(a, ps, cf, tm,
                                                 false, out_list);
            });

        return new_lex;
    }
};

class DropColumnSubHandler : public AlterSubHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        LEX *new_lex = copy(lex);
        const std::string &table =
            lex->select_lex.table_list.first->table_name;
        const std::string &dbname =
            lex->select_lex.table_list.first->db;

        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a);
        
        // Get the column drops.
        auto drop_it =
            List_iterator<Alter_drop>(lex->alter_info.drop_list);
        List<Alter_drop> col_drop_list =
            filterList(drop_it,
                [](Alter_drop *adrop) {
                    return Alter_drop::COLUMN == adrop->type;
                });

        // Rewrite and Remove.
        drop_it = List_iterator<Alter_drop>(col_drop_list);
        new_lex->alter_info.drop_list =
            reduceList<Alter_drop>(drop_it, List<Alter_drop>(),
                [table, dbname, &a, this] (List<Alter_drop> out_list,
                                           Alter_drop *adrop) {
                    FieldMeta *fm = a.getFieldMeta(table, adrop->name);
                    TableMeta *tm = a.getTableMeta(table);
                    List<Alter_drop> lst = this->rewrite(fm, adrop);
                    out_list.concat(&lst);
                    // FIXME: Slow.
                    Delta d(Delta::DELETE, fm, tm, tm->getKey(fm));
                    a.deltas.push_back(d);
                    return out_list; /* lambda */
                });

        return new_lex;
    }

    List<Alter_drop> rewrite(FieldMeta *fm, Alter_drop *adrop) const
    {
        List<Alter_drop> out_list;
        THD *thd = current_thd;

        // Rewrite each onion column.
        for (auto onion_pair : fm->children) {
            Alter_drop *new_adrop = adrop->clone(thd->mem_root);
            OnionMeta *om = onion_pair.second;
            new_adrop->name = thd->strdup(om->getAnonOnionName().c_str());
            out_list.push_back(new_adrop);
        }

        // Rewrite the salt column.
        if (fm->has_salt) {
            Alter_drop *new_adrop = adrop->clone(thd->mem_root);
            new_adrop->name =
                thd->strdup(fm->getSaltName().c_str());
            out_list.push_back(new_adrop);
        }

        return out_list;
    }
};

class ChangeColumnSubHandler : public AlterSubHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        assert(false);
    }
};

class ForeignKeySubHandler : public AlterSubHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        assert(false);
    }
};

class AddIndexSubHandler : public AlterSubHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        const std::string &table =
            lex->select_lex.table_list.first->table_name;
        LEX *new_lex = copy(lex);

        TableMeta *tm = a.getTableMeta(table);

        // -----------------------------
        //         Rewrite TABLE
        // -----------------------------
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a);

        // Add each new index.
        auto key_it =
            List_iterator<Key>(lex->alter_info.key_list);
        new_lex->alter_info.key_list = 
            reduceList<Key>(key_it, List<Key>(),
                [&table, &tm, &a] (List<Key> out_list, Key *key) {
                    // -----------------------------
                    //         Rewrite INDEX
                    // -----------------------------
                    auto new_keys = rewrite_key(table, key, a);
                    out_list.concat(vectorToList(new_keys));

                    return out_list;
            });
 
        return new_lex;
    }
};

class DropIndexSubHandler : public AlterSubHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        LEX *new_lex = copy(lex);
        const std::string &table =
            lex->select_lex.table_list.first->table_name;
        const std::string &dbname =
            lex->select_lex.table_list.first->db;
        TableMeta *tm = a.getTableMeta(table);
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a);

        // Get the key drops.
        auto drop_it =
            List_iterator<Alter_drop>(lex->alter_info.drop_list);
        List<Alter_drop> key_drop_list =
            filterList(drop_it,
                [](Alter_drop *adrop) {
                    return Alter_drop::KEY == adrop->type;
                });

        // Rewrite.
        // FIXME: Handle oDET index as well.
        drop_it =
            List_iterator<Alter_drop>(key_drop_list);
        new_lex->alter_info.drop_list =
            reduceList<Alter_drop>(drop_it, List<Alter_drop>(),
                [table, dbname, tm, &a, this] (List<Alter_drop> out_list,
                                               Alter_drop *adrop) {
                    List<Alter_drop> lst = this->rewrite(a, adrop, table);
                    out_list.concat(&lst); 
                    return out_list;
                });

        return new_lex;
    }

    List<Alter_drop> rewrite(Analysis a, Alter_drop *adrop,
                             const std::string &table) const
    {
        // Rewrite the Alter_drop data structure.
        List<Alter_drop> out_list;
        THD *thd = current_thd;
        Alter_drop *new_adrop = adrop->clone(thd->mem_root);  
        new_adrop->name =
            make_thd_string(a.getAnonIndexName(table, adrop->name));

        out_list.push_back(new_adrop);
        return out_list;
    }
};

LEX *AlterSubHandler::transformLex(Analysis &a, LEX *lex,
                                   const ProxyState &ps) const
{
    return this->rewriteAndUpdate(a, lex, ps);
}

AlterDispatcher *buildAlterSubDispatcher() {
    AlterDispatcher *dispatcher = new AlterDispatcher();
    AlterSubHandler *h;
    
    h = new AddColumnSubHandler();
    dispatcher->addHandler(ALTER_ADD_COLUMN, h);

    h = new DropColumnSubHandler();
    dispatcher->addHandler(ALTER_DROP_COLUMN, h);

    h = new ChangeColumnSubHandler();
    dispatcher->addHandler(ALTER_CHANGE_COLUMN, h);

    h = new ForeignKeySubHandler();
    dispatcher->addHandler(ALTER_FOREIGN_KEY, h);

    h = new AddIndexSubHandler();
    dispatcher->addHandler(ALTER_ADD_INDEX, h);

    h = new DropIndexSubHandler();
    dispatcher->addHandler(ALTER_DROP_INDEX, h);

    return dispatcher;
}

