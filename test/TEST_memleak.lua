package.path = package.path .. ";;test/?.lua"
require "is_external"

local c    = 200;
local req  = 500000;
local tbl  = "memleak";

function init_memleak_tbl()
    drop_table(tbl);
    create_table(tbl, "id INT, page_no INT, msg TEXT");
    create_index("ind_ml_p", tbl, "page_no");
    local icmd = '../gen-benchmark -q -c ' .. c ..' -n ' .. req ..
                 ' -s -A OK ' .. 
                 ' -Q INSERT INTO memleak VALUES ' .. 
                 '"(000000000001,1,\'pagename_000000000001\')"';
    local x   = socket.gettime()*1000;
    print ('executing: (' .. icmd .. ')');
    os.execute(icmd);
    is_external.print_diff_time('time: (' .. icmd .. ')', x);
    return "+OK";
end

function delete_memleak_tbl()
    local icmd = '../gen-benchmark -q -c ' .. c ..' -n ' .. req ..
                 ' -s -A INT ' .. 
                 ' -Q DELETE FROM memleak WHERE id=000000000001';
    local x   = socket.gettime()*1000;
    print ('executing: (' .. icmd .. ')');
    os.execute(icmd);
    is_external.print_diff_time('time: (' .. icmd .. ')', x);
    return "+OK";
end
if is_external.yes == 1 then
    print (init_memleak_tbl());
    local t = desc(tbl);
    for k,v in pairs(t) do print (v); end
    print (delete_memleak_tbl());
    t = desc(tbl);
    for k,v in pairs(t) do print (v); end
end