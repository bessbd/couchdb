% Licensed under the Apache License, Version 2.0 (the "License"); you may not
% use this file except in compliance with the License. You may obtain a copy of
% the License at
%
%   http://www.apache.org/licenses/LICENSE-2.0
%
% Unless required by applicable law or agreed to in writing, software
% distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
% WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
% License for the specific language governing permissions and limitations under
% the License.

-module(couch_bt_engine_compactor_active_tasks_tests).


-include_lib("couch/include/couch_eunit.hrl").
-include_lib("couch/include/couch_db.hrl").


-define(NUM_DOCS, 5000).
-define(CRASH_AT_DOC, 500).
-define(WAIT_DELAY_COUNT, 50).
-define(DELAY, 100).


setup() ->
    DbName = ?tempdb(),
    {ok, Db} = couch_db:create(DbName, [?ADMIN_CTX]),
    ok = couch_db:close(Db),
    create_docs(Db, ?NUM_DOCS),
    meck:new(couch_bt_engine_compactor, []),
    meck:expect(couch_bt_engine_compactor, update_compact_task,
        fun(NumChanges) ->
            meck:passthrough([NumChanges]),
            meck:expect(couch_bt_engine_compactor, update_compact_task,
                fun(NumChanges2) ->
                    meck:passthrough([NumChanges2])

%%                    erlang:error(kaboom),

                end)
        end),
    DbName.


teardown(DbName) when is_binary(DbName) ->
    couch_server:delete(DbName, [?ADMIN_CTX]),
    meck:unload(),
    ok.


compaction_resume_test_() ->
    {
        setup,
        fun test_util:start_couch/0,
        fun test_util:stop_couch/1,
        {
            foreach,
            fun setup/0,
            fun teardown/1,
            [
                fun compaction_interrupted_active_tasks/1
            ]
        }
    }.


compaction_interrupted_active_tasks(DbName) ->
    ?_test(begin
        compact_db(DbName)
    end).


create_docs(_Db, NumDocs) when NumDocs =< 0 ->
    ok;
create_docs(DbName, NumDocs) ->
    couch_util:with_db(DbName, fun(Db) ->
        {ok, _} = couch_db:update_docs(Db, [couch_doc:from_json_obj({[
            {<<"_id">>, list_to_binary(io_lib:format("doc~p", [N]))},
            {<<"value">>, N}
        ]}) || N <- lists:seq(1, NumDocs)])
    end).



compact_db(DbName) ->
    couch_util:with_db(DbName, fun(Db) ->
        {ok, _} = couch_db:start_compact(Db)
    end),
    wait_db_compact_done(DbName, ?WAIT_DELAY_COUNT).


wait_db_compact_done(_DbName, 0) ->
    Failure = [
        {module, ?MODULE},
        {line, ?LINE},
        {reason, "DB compaction failed to finish"}
    ],
    erlang:error({assertion_failed, Failure});
wait_db_compact_done(DbName, N) ->
    IsDone = couch_util:with_db(DbName, fun(Db) ->
        not is_pid(couch_db:get_compactor_pid(Db))
                                        end),
    if IsDone -> ok; true ->
        timer:sleep(?DELAY),
        wait_db_compact_done(DbName, N - 1)
    end.
