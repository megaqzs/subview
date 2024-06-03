-- subview interface script
local socket = require("posix.sys.socket")
local unistd = require("posix.unistd")

local SUBVIEW_SOCK = os.getenv("SUBVIEW_SOCK") or ("/var/run/user/" .. unistd.geteuid() .. "/subview")


local running = false
local fd = -1

local function sendSubs(fd, file_length)

	if running then
        local curr_text = mp.get_property("sub-text")
        if (curr_text) then
            assert(socket.send(fd, curr_text:gsub("^%s*(.-)%s*$", "%1") .. "\0"))
        else
            assert(socket.send(fd, "\0"))
        end
        -- Callback
        mp.add_timeout(0.1, function() sendSubs(fd, file_length) end)
	end
end

local function run()
    local file_length = mp.get_property_number('duration/full') * 1000
    fd = assert(socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0))
    assert(socket.connect(fd, {family = socket.AF_UNIX, path = SUBVIEW_SOCK}))
    sendSubs(fd, file_length)
end

local function stop()
    if fd > 0 then
        assert(socket.send(fd, "\0"))
        unistd.close(fd)
        fd = -1
    end
end

local function toggle()

	if running then
		running = false
		mp.commandv('show-text', 'Subview subtitles hidden')
		mp.unregister_event("start-file", run)
		mp.unregister_event('end-file', stop)

		stop()

	else
		running = true
		mp.commandv('show-text', 'Subview subtitles visible')
		mp.register_event("start-file", run)
		mp.register_event('end-file', stop)

		run()
	end

end

mp.add_key_binding('ctrl+v', 'subview_subs', toggle)
