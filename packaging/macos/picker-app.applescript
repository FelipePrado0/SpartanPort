-- SpartanPort (macOS): pick the render resolution, then start the game without Terminal.
-- packaging/macos/make-app.sh builds the app from this file.
property portDir : "@PORT_DIR@" -- filled in by make-app.sh

on run
	set iniPath to portDir & "/spartanport.ini"
	set logPath to portDir & "/out/launcher-app.log"
	set shellEnv to "export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH; cd " & quoted form of portDir & "; "

	if (do shell script "pgrep -f 'out/spartanport-probe' >/dev/null && echo yes || echo no") is "yes" then
		display dialog "SpartanPort is already running." buttons {"OK"} default button "OK" with title "SpartanPort" with icon caution
		return
	end if

	set labels to {"720p  (recommended, about 60 FPS)", "1080p  (sharper, about 40 FPS)", "480p  (fastest, lowest detail)"}
	set values to {"720p", "native", "480p"}

	-- Preselect the resolution saved last time.
	set saved to do shell script "grep -E '^render_resolution=' " & quoted form of iniPath & " 2>/dev/null | tail -1 | cut -d= -f2 || true"
	set preselected to item 1 of labels
	repeat with i from 1 to count of values
		if item i of values is saved then set preselected to item i of labels
	end repeat

	set picked to choose from list labels with title "SpartanPort" with prompt "Render resolution:" default items {preselected} OK button name "Play" cancel button name "Quit"
	if picked is false then return
	set chosen to item 1 of picked
	set resolution to "720p"
	repeat with i from 1 to count of labels
		if item i of labels is chosen then set resolution to item i of values
	end repeat

	-- Save it in spartanport.ini (the same setting the launcher and the in-game menu change).
	do shell script "f=" & quoted form of iniPath & "; touch \"$f\"; if grep -q '^render_resolution=' \"$f\"; then sed -i '' 's/^render_resolution=.*/render_resolution=" & resolution & "/' \"$f\"; else echo 'render_resolution=" & resolution & "' >> \"$f\"; fi"

	-- Start the game in the background, as the launcher's PLAY button does.
	do shell script shellEnv & "mkdir -p out; (nohup python3 " & quoted form of (portDir & "/run.py") & " > " & quoted form of logPath & " 2>&1 &); true"

	-- Report a start that fails (the game window normally opens within ~15 s).
	repeat 40 times
		delay 1
		if (do shell script "pgrep -f 'out/spartanport-probe' >/dev/null && echo yes || echo no") is "yes" then return
		if (do shell script "pgrep -f " & quoted form of (portDir & "/run.py") & " >/dev/null && echo yes || echo no") is "no" then exit repeat
	end repeat
	if (do shell script "pgrep -f 'out/spartanport-probe' >/dev/null && echo yes || echo no") is "no" then
		set tailText to do shell script "tail -n 12 " & quoted form of logPath & " 2>/dev/null || true"
		display dialog "SpartanPort did not start." & return & return & tailText & return & return & "Full log: " & logPath buttons {"OK"} default button "OK" with title "SpartanPort" with icon stop
	end if
end run
