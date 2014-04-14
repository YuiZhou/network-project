#! /usr/bin/env ruby
#
# 15-441 Simple IRC test 
#
# You must have passed the previous tests.  For example, you cannot
# pass the "PART" test if your "JOIN" test failed.
# For this reason, the script exits once the server fails a test.
# If you fail a specific test, we suggest you modify this code 
# to print out exactly # what is going on, or perhaps remove tests that
# fail so that you can test other, unrelated tests if you simply haven't
# implemented a particular bit of functionality yet (e.g., you might
# remove the WHO or LIST tests to test PART).
# 
# Enjoy!
#
# - 15-441 Staff

## SKELETON STOLEN FROM http://www.bigbold.com/snippets/posts/show/1785
require 'socket'

$SERVER = "127.0.0.1"
$PORT = 34102  ########## DONT FORGET TO CHANGE THIS

#if ARGV.size == 0
#  puts "Usage: ./simpleirc.rb server_port [server_ip_addr]"
#  exit
#elsif ARGV.size >= 1
#
#  begin
#    $PORT = Integer(ARGV[0])
#  rescue
#    puts "The port number must be an integer!"
#    exit
#  end
#end

if ARGV.size >= 2
  $SERVER = ARGV[1].to_s()
end

puts "Server address - " + $SERVER + ":" + $PORT.to_s()

class IRC

  def initialize(server, port, nick, channel)
    @server = server
    @port = port
    @nick = nick
    @channel = channel
  end

  def recv_data_from_server (timeout)
    pending_event = Time.now.to_i
    received_data = Array.new
    k = 0 
    flag = 0
    while flag == 0
      ## check for timeout
      time_elapsed = Time.now.to_i - pending_event
      if (time_elapsed > timeout)
        flag = 1
      end 
      ready = select([@irc], nil, nil, 0.0001)
      next if !ready
      for s in ready[0]
        if s == @irc then
          next if @irc.eof
          s = @irc.gets
          received_data[k] = s
          k= k + 1
        end
      end
    end
    return received_data
  end

  def test_silence(timeout)
    data=recv_data_from_server(timeout)
    if (data.size > 0)
      return false
    else
      return true
    end
  end

  def send(s)
    # Send a message to the irc server and print it to the screen
    puts "--> #{s}"
    @irc.send "#{s}\n", 0 
  end

  def connect()
    # Connect to the IRC server
    @irc = TCPSocket.open(@server, @port)
  end

  def disconnect()
    @irc.close
  end

  def send_nick(s)
    send("NICK #{s}")
  end

  def duplicate_nick(s)
    send("NICK #{s}")

    data = recv_data_from_server(1);
    if(data[0] =~ /^NICKNAMEINUSE/)
      puts "\tNick name in use detected"
      return true
    else
      puts "\tNick name in use not detected"
      return false
    end
  end

  def send_user(s)
    send("USER #{s}")
  end

  def get_motd
    data = recv_data_from_server(1)
    ## CHECK data here

    if(data[0] =~ /^:[^ ]+ *375 *gnychis *:- *[^ ]+ *Message of the day - *.\n/)
      puts "\tRPL_MOTDSTART 375 correct"
    else
      puts "\tRPL_MOTDSTART 375 incorrect"
      return false
    end

    k = 1
    while ( k < data.size-1)

      if(data[k] =~ /:[^ ]+ *372 *gnychis *:- *.*/)
        puts "\tRPL_MOTD 372 correct"
      else
        puts "\tRPL_MOTD 372 incorrect"
        return false
      end
      k = k + 1
    end

    if(data[data.size-1] =~ /:[^ ]+ *376 *gnychis *:End of \/MOTD command/)
      puts "\tRPL_ENDOFMOTD 376 correct"
    else
      puts "\tRPL_ENDOFMOTD 376 incorrect"
      return false
    end

    return true
  end

  def send_privmsg(s1, s2)
    send("PRIVMSG #{s1} :#{s2}")
  end

  def raw_join_channel(joiner, channel)
    send("JOIN #{channel}")
    ignore_reply()
  end

  def join_channel(joiner, channel)
    send("JOIN #{channel}")

    data = recv_data_from_server(1);
    if(data[0] =~ /^:#{joiner}.*JOIN *#{channel}/)
      puts "\tJOIN echoed back"
    else
      puts "\tJOIN was not echoed back to the client"
      return false
    end

    if(data[1] =~ /^:[^ ]+ *353 *#{joiner} *= *#{channel} *:.*#{joiner}/)
      puts "\tRPL_NAMREPLY 353 correct"
    else
      puts "\tRPL_NAMREPLY 353 incorrect"
      return false
    end

    if(data[2] =~ /^:[^ ]+ *366 *#{joiner} *#{channel} *:End of \/NAMES list/)
      puts "\tRPL_ENDOFNAMES 366 correct"
    else
      puts "\tRPL_ENDOFNAMES 366 incorrect"
      return false
    end

    return true
  end

  def who(s)
    send("WHO #{s}")

    data = recv_data_from_server(1);

    if(data[0] =~ /^:[^ ]+ *352 *gnychis *#{s} *please *[^ ]+ *[^ ]+ *gnychis *H *:0 *The MOTD/)
      puts "\tRPL_WHOREPLY 352 correct"
    else
      puts "\tRPL_WHOREPLY 352 incorrect"
      return false
    end

    if(data[1] =~ /^:[^ ]+ *315 *gnychis *#{s} *:End of \/WHO list/)
      puts "\tRPL_ENDOFWHO 315 correct"
    else
      puts "\tRPL_ENDOFWHO 315 incorrect"
      return false
    end
    return true
  end

  def list
    send("LIST")

    data = recv_data_from_server(1);
    if(data[0] =~ /^:[^ ]+ *321 *gnychis *Channel *:Users Name/)
      puts "\tRPL_LISTSTART 321 correct"
    else
      puts "\tRPL_LISTSTART 321 incorrect"
      return false
    end

    if(data[1] =~ /^:[^ ]+ *322 *gnychis *#linux.*1/)
      puts "\tRPL_LIST 322 correct"
    else
      puts "\tRPL_LIST 322 incorrect"
      return false
    end

    if(data[2] =~ /^:[^ ]+ *323 *gnychis *:End of \/LIST/)
      puts "\tRPL_LISTEND 323 correct"
    else
      puts "\tRPL_LISTEND 323 incorrect"
      return false
    end

    return true
  end

  def checkmsg(from, to, msg)
    reply_matches(/^:#{from} *PRIVMSG *#{to} *:#{msg}/, "PRIVMSG")
  end

  def check2msg(from, to1, to2, msg)
    data = recv_data_from_server(1);
    if((data[0] =~ /^:#{from} *PRIVMSG *#{to1} *:#{msg}/ && data[1] =~ /^:#{from} *PRIVMSG *#{to2} *:#{msg}/) ||
       (data[1] =~ /^:#{from} *PRIVMSG *#{to1} *:#{msg}/ && data[0] =~ /^:#{from} *PRIVMSG *#{to2} *:#{msg}/))
       puts "\tPRIVMSG to #{to1} and #{to2} correct"
      return true
    else
      puts "\tPRIVMSG to #{to1} and #{to2} incorrect"
      return false
    end
  end

  def check_echojoin(from, channel)
    reply_matches(/^:#{from}.*JOIN *#{channel}/,
                  "Test if first client got join echo")
  end

  def part_channel(parter, channel)
    send("PART #{channel}")
    reply_matches(/^:#{parter}![^ ]+@[^ ]+ *QUIT *:/)

  end

  def check_part(parter, channel)
    reply_matches(/^:#{parter}![^ ]+@[^ ]+ *QUIT *:/)
  end

  def ignore_reply
    recv_data_from_server(1)
  end

  def reply_matches(rexp, explanation = nil)
    data = recv_data_from_server(1)
    if (rexp =~ data[0])
      puts "\t #{explanation} correct" if explanation
      return true
    else
      puts "\t #{explanation} incorrect: #{data[0]}" if explanation
      return false
    end
  end

end


def write_config()
    File.open("grading.conf", "w") { |afile|
	afile.puts "1 127.0.0.1 34100 34101 34102"
    }
end

def spawn_server(config,id)
afile = File.open(config, "r")
afile.each_line do |line|
	line.chomp!
	(nodeid, ip, lport, dport, sport)=line.split
	if(nodeid.to_i == id)  # dont forget to convert to int
		puts "./sircd #{id} #{config} &> /dev/null &"
		system("./sircd #{id} #{config} > sircd#{id}.log &")
		return 1 
	end
end
end

##
# The main program.  Tests are listed below this point.  All tests
# should call the "result" function to report if they pass or fail.
##

$total_points = 0

def test_name(n)
  puts "////// #{n} \\\\\\\\\\\\"
  return n
end

def result(n, passed_exp, failed_exp, passed, points)
  explanation = nil
  if (passed)
    print "(+) #{n} passed"
    $total_points += points
    explanation = passed_exp
  else
    print "(-) #{n} failed"
    explanation = failed_exp
  end

  if (explanation)
    puts ": #{explanation}"
  else
    puts ""
  end
end

def eval_test(n, passed_exp, failed_exp, passed, points = 1)
  result(n, passed_exp, failed_exp, passed, points)
  exit(0) if !passed
end

def createIrc(name)
  irc = IRC.new($SERVER, $PORT, '', '')
  irc.connect()
  irc.send_nick(name)
  irc.send_user("please give me :The MOTD2")
  irc.ignore_reply()
  return irc
end

#### WRITE CONFIG FILE
#
# YOU mAY EDIT THE PORT NUMBERS HERE ONLY
# (to avoid port collisions while testing on andrew)
write_config()

begin

spawn_server("grading.conf", 1)

# Let the servers settle
puts "Letting the servers settle."
sleep(3)

irc = IRC.new($SERVER, $PORT, '', '')
irc.connect()

  ########## TEST NICK COMMAND ##########################
  # The RFC states that there is no response to a NICK command,
  # so we test for this.
  tn = test_name("NICK")
  irc.send_nick("gnychis")
  puts "<-- Testing for silence (1 seconds)..."

  eval_test(tn, nil, nil, irc.test_silence(1))


  ############## TEST USER COMMAND ##################
  # The RFC states that there is no response on a USER,
  # so we disconnect first (otherwise the full registration
  # of NICK+USER would give us an MOTD), then check
  # for silence
  tn = test_name("USER")

  puts "Disconnecting and reconnecting to IRC server\n"
  irc.disconnect()
  irc.connect()

  irc.send_user("please give me :The MOTD")
  puts "<-- Testing for silence (1 seconds)..."

  eval_test(tn, nil, "should not return a response on its own", 
            irc.test_silence(1))

  ############# TEST FOR REGISTRATION ##############
  # A NICK+USER is a registration that triggers the
  # MOTD.  This test sends a nickname to complete the registration,
  # and then checks for the MOTD.
  tn = test_name("Registration")
  irc.send_nick("gnychis")
  puts "<-- Listening for MOTD...";

  eval_test(tn, nil, nil, irc.get_motd())

  ############## TEST JOINING ####################
  # We join a channel and make sure the client gets
  # his join echoed back (which comes first), then
  # gets a names list.
  tn = test_name("JOIN")
  eval_test(tn, nil, nil,
            irc.join_channel("gnychis", "#linux"))

  ############## WHO ####################
  # Who should list everyone in a channel
  # or everyone on the server.  We are only
  # checking WHO <channel>.
  # The response should follow the RFC.
  tn = test_name("WHO")
  eval_test(tn, nil, nil, irc.who("#linux"))

  ############## LIST ####################
  # LIST is used to check all the channels on a server.
  # The response should include #linux and its format should follow the RFC.
  tn = test_name("LIST")
  eval_test(tn, nil, nil, irc.list())

  ############## PRIVMSG ###################
  # Connect a second client that sends a message to the original client.
  # Test that the original client receives the message.
  tn = test_name("PRIVMSG")
  irc2 = IRC.new($SERVER, $PORT, '', '')
  irc2.connect()
  irc2.send_nick("gnychis2")
  irc2.send_user("please give me :The MOTD2")
  msg = "clown hat curly hair smiley face"
  irc2.send_privmsg("gnychis", msg)
  eval_test(tn, nil, nil, irc.checkmsg("gnychis2", "gnychis", msg))

  ############## ECHO JOIN ###################
  # When another client joins a channel, all clients
  # in that channel should get :newuser JOIN #channel
  tn = test_name("ECHO ON JOIN")
  # "raw" means no testing of responses done
  irc2.raw_join_channel("gnychis2", "#linux")
  irc2.ignore_reply()
  eval_test(tn, nil, nil, irc.check_echojoin("gnychis2", "#linux"))


  ############## MULTI-TARGET PRIVMSG ###################
  # A client should be able to send a single message to
  # multiple targets, with ',' as a delimiter.
  # We use client2 to send a message to gnychis and #linux.
  # Both should receive the message.
  tn = test_name("MULTI-TARGET PRIVMSG")
  msg = "awesome blossom with extra awesome"
  irc2.send_privmsg("gnychis,#linux", msg)
  eval_test(tn, nil, nil, irc.check2msg("gnychis2", "gnychis", "#linux", msg))
  irc2.ignore_reply()

  ############## PART ###################
  # When a client parts a channel, a QUIT message
  # is sent to all clients in the channel, including
  # the client that is parting.
  tn = test_name("PART")
  eval_test("PART echo to self", nil, nil,
            irc2.part_channel("gnychis2", "#linux"),
            0) # note that this is a zero-point test!

  eval_test("PART echo to other clients", nil, nil,
            irc.check_part("gnychis2", "#linux"))

  ## Your tests go here!
  # 
  # Things you might want to test:
  #  - Multiple clients in a channel		-----okay	marks:1
  #  - Abnormal messages of various sorts
  #  - Clients that misbehave/disconnect/etc.	-----okay	marks:1
  #  - Lots and lots of clients			-----okay	marks:1
  #  - Lots and lots of channel switching	-----okay	marks:2
  #  - etc.
  ##

  ############## Clients Disconnect ###################
  # When a client disconnect a channel, a QUIT message
  # is sent to all clients in the channel, including
  # the client that is parting.
  tn = test_name("Clients Disconnect")

  irc2.disconnect()
  irc2.connect()
  irc2.send_nick("gnychis3")
  irc2.send_user("please give me :The MOTD2")
  irc2.raw_join_channel("gnychis3", "#linux")
  irc2.ignore_reply()
  irc2.disconnect()
  eval_test(tn, nil, nil, irc.check_echojoin("gnychis3", "#linux"))
  
  ############## Nick name duplicate detect ###################
  tn = test_name("Nick name duplicate detect")
  irc2.connect()
  eval_test(tn, nil, nil, irc2.duplicate_nick("gnychis"))

  ############## Multiple clients in a channel ###################
  tn = test_name("Multiple clients in a channel")
  irc3 = createIrc("gnychis3")
  irc4 = createIrc("gnychis4")
  irc5 = createIrc("gnychis5")
  irc6 = createIrc("gnychis6")
  irc7 = createIrc("gnychis7")
  irc8 = createIrc("gnychis8")

  irc3.join_channel("gnychis3", "#linux")
  irc4.join_channel("gnychis4", "#linux")
  irc3.check_echojoin("gnychis4", "#linux")
  irc5.join_channel("gnychis5", "#linux")
  irc3.check_echojoin("gnychis5", "#linux")
  irc4.check_echojoin("gnychis5", "#linux")
  irc6.join_channel("gnychis6", "#linux")
  irc3.check_echojoin("gnychis6", "#linux")
  irc4.check_echojoin("gnychis6", "#linux")
  irc5.check_echojoin("gnychis6", "#linux")
  irc7.join_channel("gnychis7", "#linux")
  irc3.check_echojoin("gnychis7", "#linux")
  irc4.check_echojoin("gnychis7", "#linux")
  irc5.check_echojoin("gnychis7", "#linux")
  irc6.check_echojoin("gnychis7", "#linux")
  irc8.join_channel("gnychis8", "#linux")
  irc3.check_echojoin("gnychis8", "#linux")
  irc4.check_echojoin("gnychis8", "#linux")
  irc5.check_echojoin("gnychis8", "#linux")
  irc6.check_echojoin("gnychis8", "#linux")
  irc7.check_echojoin("gnychis8", "#linux")
  msg = "clown hat curly hair smiley face"
  irc3.send_privmsg("#linux", msg)
  eval_test(tn, nil, nil, irc3.checkmsg("gnychis3", "#linux", msg), 0)
  eval_test(tn, nil, nil, irc4.checkmsg("gnychis3", "#linux", msg), 0)
  eval_test(tn, nil, nil, irc5.checkmsg("gnychis3", "#linux", msg), 0)
  eval_test(tn, nil, nil, irc6.checkmsg("gnychis3", "#linux", msg), 0)
  eval_test(tn, nil, nil, irc7.checkmsg("gnychis3", "#linux", msg), 0)
  eval_test(tn, nil, nil, irc8.checkmsg("gnychis3", "#linux", msg), 2)

  ############## Channel Switching ###################
  tn = test_name("Channel Switching")
  irc3.raw_join_channel("gnychis3", "#networks")
  irc3.ignore_reply()
  irc4.check_part("gnychis3", "#linux")
  irc5.check_part("gnychis3", "#linux")
  irc6.check_part("gnychis3", "#linux")
  irc7.check_part("gnychis3", "#linux")
  irc8.check_part("gnychis3", "#linux")
  irc5.raw_join_channel("gnychis5", "#networks")
  irc5.ignore_reply()
  irc3.check_echojoin("gnychis5", "#networks")
  irc4.check_part("gnychis5", "#linux")
  irc6.check_part("gnychis5", "#linux")
  irc7.check_part("gnychis5", "#linux")
  irc8.check_part("gnychis5", "#linux")
  irc7.raw_join_channel("gnychis7", "#networks")
  irc7.ignore_reply()
  irc3.check_echojoin("gnychis7", "#networks")
  irc4.check_part("gnychis7", "#linux")
  irc5.check_echojoin("gnychis7", "#networks")
  irc6.check_part("gnychis7", "#linux")
  irc8.check_part("gnychis7", "#linux")
  msg = "clown hat curly hair smiley face"
  irc3.send_privmsg("#linux", msg)
  eval_test(tn, nil, nil, irc3.test_silence(1), 0)
  eval_test(tn, nil, nil, irc4.checkmsg("gnychis3", "#linux", msg), 0)
  eval_test(tn, nil, nil, irc5.test_silence(1), 0)
  eval_test(tn, nil, nil, irc6.checkmsg("gnychis3", "#linux", msg), 0)
  eval_test(tn, nil, nil, irc7.test_silence(1), 0)
  eval_test(tn, nil, nil, irc8.checkmsg("gnychis3", "#linux", msg))
  msg = "clown hat curly hair smiley face 2"
  irc3.send_privmsg("#networks", msg)
  eval_test(tn, nil, nil, irc3.checkmsg("gnychis3", "#networks", msg), 0)
  eval_test(tn, nil, nil, irc4.test_silence(1), 0)
  eval_test(tn, nil, nil, irc5.checkmsg("gnychis3", "#networks", msg), 0)
  eval_test(tn, nil, nil, irc6.test_silence(1), 0)
  eval_test(tn, nil, nil, irc7.checkmsg("gnychis3", "#networks", msg), 0)
  eval_test(tn, nil, nil, irc8.test_silence(1))



rescue Interrupt
rescue Exception => detail
  puts detail.message()
  print detail.backtrace.join("\n")
ensure
  irc.disconnect()
  system("killall srouted")
  system("killall sircd")
  puts "Your score: #{$total_points} / 16"
  puts ""
  puts "Good luck with the rest of the project!"
end
