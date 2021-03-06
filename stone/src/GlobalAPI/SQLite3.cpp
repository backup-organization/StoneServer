#include "common.h"
#include <sqlite3.h>
#include <stone/utils.h>

#include <map>

namespace GlobalAPI {

using StatmentMap = std::map<std::string, sqlite3_stmt *>;

static void bind_sqlite3_open(FunctionCallbackInfo<Value> const &info) {
  auto iso = info.GetIsolate();
  Isolate::Scope isos{ iso };
  if (!info.IsConstructCall()) {
    iso->ThrowException(Exception::TypeError(ToJS("Use the new operator to create new Database objects")));
  } else if (!info[0]->IsString()) {
    iso->ThrowException(Exception::TypeError(ToJS("Use new SQLite3(path: string)")));
  } else {
    auto dbname = info[0]->ToString(iso) >> V8Str;
    sqlite3 *db;
    if (sqlite3_open(dbname.c_str(), &db) != SQLITE_OK) {
      iso->ThrowException(ToJS("cannot open the database"));
    } else {
      info.Holder()->SetAlignedPointerInInternalField(0, db);
      info.Holder()->SetInternalField(1, v8::True(iso));
      info.Holder()->SetAlignedPointerInInternalField(2, new StatmentMap);
      info.GetReturnValue().Set(info.Holder());
    }
  }
}

static void bind_sqlite3_valid(FunctionCallbackInfo<Value> const &info) { info.GetReturnValue().Set(info.Holder()->GetInternalField(1)); }

static void bind_sqlite3_close(FunctionCallbackInfo<Value> const &info) {
  auto iso = info.GetIsolate();
  Isolate::Scope isos{ iso };
  if (info.Holder()->GetInternalField(1)->IsTrue()) {
    auto db = (sqlite3 *)info.Holder()->GetAlignedPointerFromInternalField(0);
    sqlite3_close(db);
    info.Holder()->SetInternalField(1, v8::False(iso));
    delete (StatmentMap *)info.Holder()->GetAlignedPointerFromInternalField(2);
  }
}

static void bind_sqlite3_exec(FunctionCallbackInfo<Value> const &info) {
  auto iso = info.GetIsolate();
  Isolate::Scope isos{ iso };
  if (info.Holder()->GetInternalField(1)->IsTrue()) {
    auto db      = (sqlite3 *)info.Holder()->GetAlignedPointerFromInternalField(0);
    char *errmsg = nullptr;
    if (info.Length() == 1 && info[0]->IsString()) {
      auto ret = sqlite3_exec(db, info[0] >> V8Str >> CStr, nullptr, nullptr, &errmsg);
      if (errmsg) iso->ThrowException(Exception::Error(ToJS(errmsg)));
      info.GetReturnValue().Set(v8::Integer::New(iso, ret));
    } else if (info.Length() == 2 && info[0]->IsString() && info[1]->IsFunction()) {
      TryCatch tc{ iso };
      auto tuple = std::tuple{ iso, Local<Function>(info[1]), &tc };
      auto ret   = sqlite3_exec(db, info[0] >> V8Str >> CStr,
                              +[](void *data, int num, char **values, char **keys) -> int {
                                auto &[iso, fn, tc] = *(decltype(tuple) *)data;
                                auto arg            = Object::New(iso);
                                for (int i = 0; i < num; i++) arg->Set(ToJS(keys[i]), ToJS(values[i]));
                                fn->Call(v8::Undefined(iso), 1, (Local<Value> *)&arg);
                                return tc->HasCaught();
                              },
                              &tuple, &errmsg);
      if (tc.HasCaught())
        tc.ReThrow();
      else if (errmsg)
        iso->ThrowException(Exception::Error(ToJS(errmsg)));
      else if (ret != SQLITE_OK)
        iso->ThrowException(Exception::Error(ToJS(sqlite3_errstr(ret))));
    } else {
      iso->ThrowException(Exception::TypeError(ToJS("Use exec(sql: string, callback?: (line: { [index: string]: string })): number")));
    }
    if (errmsg) sqlite3_free(errmsg);
  } else {
    iso->ThrowException(Exception::Error(ToJS("Database is closed")));
  }
}

static sqlite3_stmt *create_stmt(StatmentMap *smap, sqlite3 *db, std::string const &sql) {
  sqlite3_stmt *ret;
  char const *tail;
  auto status = sqlite3_prepare_v2(db, sql.c_str(), sql.size(), &ret, &tail);
  if (status == SQLITE_OK) {
    smap->emplace(sql, ret);
    return ret;
  }
  throw sqlite3_errmsg(db);
}

static void fill_stmt(Isolate *iso, sqlite3 *db, sqlite3_stmt *stmt, Local<Object> param) {
  auto keys = param->GetOwnPropertyNames(iso->GetCurrentContext(), (v8::PropertyFilter)(v8::SKIP_SYMBOLS | v8::ONLY_ENUMERABLE));
  for (unsigned i = 0; i < keys->Length(); i++) {
    auto key  = keys->Get(i);
    int numid = -1;
    if (key->IsNumber()) {
      numid = fromJS<int>(iso, key) + 1;
    } else {
      numid = sqlite3_bind_parameter_index(stmt, key >> V8Str >> CStr);
    }
    if (numid == 0) { throw Exception::ReferenceError(ToJS(strformat("param`%s` not found", key->ToString(iso) >> V8Str >> CStr))); }
    auto value = param->Get(key);
    if (value->IsArrayBuffer()) {
      auto buffer = Local<ArrayBuffer>(value)->GetContents();
      sqlite3_bind_blob(stmt, numid, buffer.Data(), buffer.ByteLength(), SQLITE_TRANSIENT);
    } else if (value->IsString()) {
      sqlite3_bind_text(stmt, numid, fromJS<std::string>(iso, value) >> CStr, -1, SQLITE_TRANSIENT);
    } else if (value->IsNumber()) {
      sqlite3_bind_double(stmt, numid, fromJS<int>(iso, value));
    } else if (value->IsNullOrUndefined()) {
      sqlite3_bind_null(stmt, numid);
    } else {
      throw Exception::TypeError(ToJS(strformat("%s is unsupported", value->TypeOf(iso) >> V8Str >> CStr)));
      return;
    }
  }
}

static void bind_sqlite3_query(FunctionCallbackInfo<Value> const &info) {
  auto iso = info.GetIsolate();
  Isolate::Scope isos{ iso };
  try {

    if (info.Holder()->GetInternalField(1)->IsTrue()) {
      auto db   = (sqlite3 *)info.Holder()->GetAlignedPointerFromInternalField(0);
      auto smap = (StatmentMap *)info.Holder()->GetAlignedPointerFromInternalField(2);
      if (info.Length() == 2 && info[0]->IsString() && info[1]->IsObject()) {
        auto sql     = info[0] >> V8Str >> StdStr;
        auto param   = Local<Object>(info[1]);
        auto it_stmt = smap->find(sql);
        auto stmt    = it_stmt == smap->end() ? create_stmt(smap, db, sql) : it_stmt->second;
        FailGuard stmt_guard{ [&] {
          sqlite3_reset(stmt);
          sqlite3_clear_bindings(stmt);
        } };
        fill_stmt(iso, db, stmt, param);
        int ret  = 0;
        int idx  = 0;
        auto arr = Array::New(iso, 0);
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
          auto obj  = Object::New(iso);
          int count = sqlite3_column_count(stmt);
          for (int i = 0; i < count; i++) {
            auto key   = (char const *)sqlite3_column_name(stmt, i);
            auto vtype = sqlite3_column_type(stmt, i);
            Local<Value> value;
            switch (vtype) {
            case SQLITE_INTEGER: value = Integer::New(iso, sqlite3_column_int(stmt, i)); break;
            case SQLITE_FLOAT: value = Number::New(iso, sqlite3_column_double(stmt, i)); break;
            case SQLITE_TEXT: value = ToJS((char const *)sqlite3_column_text(stmt, i)); break;
            case SQLITE_BLOB: {
              auto abuffer = ArrayBuffer::New(iso, sqlite3_column_bytes(stmt, i));
              auto buffer  = abuffer->GetContents();
              memcpy(buffer.Data(), sqlite3_column_blob(stmt, i), buffer.ByteLength());
              value = abuffer;
            } break;
            case SQLITE_NULL: value = v8::Null(iso); break;
            default: value = v8::Undefined(iso); break;
            }
            obj->Set(ToJS(key), value);
          }
          arr->Set(idx++, obj);
        }
        if (ret == SQLITE_DONE) {
          info.GetReturnValue().Set(arr);
        } else {
          throw sqlite3_errmsg(db);
        }
      } else {
        throw "Use query(sql: string, params: { [index: string | number]: any }): Iterable<{ [index: string]: any }>";
      }
    } else {
      throw "Database is closed";
    }
  } catch (char const *err) { info.GetIsolate()->ThrowException(Exception::Error(ToJS(err))); } catch (Local<Value> &ex) {
    info.GetIsolate()->ThrowException(ex);
  }
}

static void bind_sqlite3_update(FunctionCallbackInfo<Value> const &info) {
  auto iso = info.GetIsolate();
  Isolate::Scope isos{ iso };
  try {

    if (info.Holder()->GetInternalField(1)->IsTrue()) {
      auto db   = (sqlite3 *)info.Holder()->GetAlignedPointerFromInternalField(0);
      auto smap = (StatmentMap *)info.Holder()->GetAlignedPointerFromInternalField(2);
      if (info.Length() == 2 && info[0]->IsString() && info[1]->IsObject()) {
        auto sql     = info[0] >> V8Str >> StdStr;
        auto param   = Local<Object>(info[1]);
        auto it_stmt = smap->find(sql);
        auto stmt    = it_stmt == smap->end() ? create_stmt(smap, db, sql) : it_stmt->second;
        FailGuard stmt_guard{ [&] {
          sqlite3_reset(stmt);
          sqlite3_clear_bindings(stmt);
        } };
        fill_stmt(iso, db, stmt, param);
        int ret = sqlite3_step(stmt);
        switch (ret) {
        case SQLITE_DONE: info.GetReturnValue().Set(Integer::New(iso, sqlite3_changes(db))); break;
        case SQLITE_ROW: throw "cannot query in this context"; break;
        default: throw sqlite3_errmsg(db);
        }
      } else {
        throw Exception::TypeError(ToJS("Use update(sql: string, params: { [index: string | number]: any }): number"));
      }
    } else {
      iso->ThrowException(Exception::Error(ToJS("Database is closed")));
    }
  } catch (char const *err) { info.GetIsolate()->ThrowException(Exception::Error(ToJS(err))); } catch (Local<Value> &ex) {
    info.GetIsolate()->ThrowException(ex);
  }
}

static Register reg([](Local<ObjectTemplate> &obj, Isolate *iso) {
  auto tmp_Database = FunctionTemplate::New(iso, bind_sqlite3_open, 1);
  tmp_Database->SetClassName(ToJS("SQLite3"));
  auto proto_Database = tmp_Database->InstanceTemplate();
  proto_Database->SetInternalFieldCount(3);
  proto_Database->SetAccessorProperty(ToJS("valid"), FunctionTemplate::New(iso, bind_sqlite3_valid));
  proto_Database->Set(ToJS("close"), FunctionTemplate::New(iso, bind_sqlite3_close));
  proto_Database->Set(ToJS("exec"), FunctionTemplate::New(iso, bind_sqlite3_exec));
  proto_Database->Set(ToJS("query"), FunctionTemplate::New(iso, bind_sqlite3_query));
  proto_Database->Set(ToJS("update"), FunctionTemplate::New(iso, bind_sqlite3_update));
  obj->Set(ToJS("SQLite3"), tmp_Database);
});

} // namespace GlobalAPI