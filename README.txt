=== Svnlib README ===

The PHP svn library (svnlib) is an all-userspace PHP library that facilitates
easy interaction with the svn binary directly from php code. The philosophy
behind the library is to replicate command line-style behavior as much as
possible; in other words, everything you know about using svn from a
shell/command line will apply to svnlib.



== BASICS: HOW TO USE IT ==

Begin by creating a new svn 'instance' - providing the path to either an svn
working copy, or an svn repository:

	$wc = new SvnWorkingCopy('/path/to/local/working/copy');

Working copies are simpler, so we'll stick with them for this example, but
repository objects are also created by passing in a path, including the
protocol:

	$repo = new SvnRepository('file:///path/to/local/repository');

Establishing the instance is rather like giving metadata for a database
connection: once the database connection data is in your SvnInstance object,
you can run as many svn commands - like database queries - against that instance
as you'd like. The programmatic flow for running commands occurs in three
stages: 

  1) Command spawning
  2) Command preparation
  3) Command execution

Commands are spawned by calling the SvnInstance::svn() method and passing in the
name of the subcommand to be invoked:

	$info = $wc->svn('info');

$info now contains an svn subcommand object (a child of the SvnCommand abstract
class; in the example, an SvnInfo object) which you 'prepare' by adding
parameters using the provided methods. The method names are all the same as the
parameters for svn subcommands you already know and love:

	$info->xml(); // turns on the '--xml' switch
	$info->incremental(); // turns on the '--incremental' switch
	$info->revision(423); // will pass in the opt '--revision 423'
	$info->depth('infinity'); // will pass in the opt '--depth infinity'
	$info->target('trunk/index.php'); // will pass in 'trunk/index.php', will be interpreted relative to wc base path
	$info->target('trunk/example.inc', 424); // will pass in 'trunk/example.inc@424', interpreted relative to wc base path

Once you're done passing in parameters, running the command is simple:

	$output = $info->execute();

The svnlib will generate a properly escaped system call in accordance with what
you prepared (in the example, `svn info --xml --incremental --revision 423 --depth infinity trunk/index.php trunk/example.php@424`),
fire the command using proc_open, then feed the results of the command back into
$output.



== MORE COMPLEX BEHAVIOR (GOODIES!) ==

The svnlib has a number of ways that its behavior can be streamlined and optimized.

= COMMAND CHAINING =

Publicly visible code in svnlib is almost entirely a fluent API; exceptions are
few and far between, mostly documented, and usually intentional (i.e., there
because fluency doesn't make sense in the situation). This means you can chain
commands together. The above $info example could be rewritten in a chain as
follows:

  $output = $info->xml()->incremental()->revision(423)->depth('infinity')
    ->target('trunk/index.php')->target('trunk/example.inc', 424)->execute();

This will generate a system call that is identical to the original example. All
of the command queuing methods are fluent, so they can be chained. Execute, by
default, is not fluent, but instead returns the generated output.

= SMART PARAMETER QUEUING/STORAGE =

Some commands can be present more than once; others only once. Handled.

= MASS TARGETING =

The --targets command line option allows you to specify a file from which svn
should read the list of targets for command execution. Svnlib leverages this
option by providing an 'aggregation' feature to the standard approach for
queuing a target (SvnCommand::target()), wherein a file for use by --targets
will be dynamically generated using the targets you pass in one at a time to
SvnCommand::target(). Aggregation is enabled by passing TRUE as the third
parameter. Our example again:

  $output = $info->xml()->inc... (same as above)
    ->target('trunk/index.php', NULL, TRUE)
    ->target('trunk/index.php', 424, TRUE)->execute();

While the output will be identical to the original example, the system call will
be slightly different:

  svn info --xml --incremental --revision 423 --depth infinity --targets /tmp/<random filename>

This approach is primarily useful in situations where you would otherwise be
queueing a lot - hundreds, thousands, more - of individual target items. Using
the aggregate approach will save on object creation. Ordinarily, a new object
would be created for every ->target(...) specified, but with aggregation, there
is only one object for the entire lot. Passing a more manageable number of
characters via the system call may also be beneficial, but I've not benchmarked
it.

Note that you can use aggregation as well as an explicit --targets file. The
command preparation logic is smart enough to know that both are present, and
will smoosh the aggregated targets with the contents of the explicit targets
file into the temp file at execution time.

= FUN WITH COMMAND SWITCHES =

The various methods for setting command parameters are handy, but aren't always
adequate. If necessary, there are some other approaches to accessing and
altering command switches.

One option is toggling certain switches, which you may need to do if you need to
change switches without knowing their setting beforehand. For that, just call
SvnCommand::toggleSwitches($bits), where $bits is a bitmask containing all the
bits that need flipping.

More common would be the need to set switches en masse. SvnCommand::cmdSwitches
is public for this exact sort of reason: you can just manipulate it directly, or
overwrite it completely with a fresh set of bits.

See the docblocks for SvnCommand and children for the constants that are used to
represent the various command switches.

= CUSTOM OUTPUT PARSERS =

Svnlib's built in output parsers are, at best, weak. While they do the handy
job of implementing some SPL interfaces that cut down on memory use, they also
present that data in a very contrived fashion that may not suit your needs.
Fortunately, they're easily overridden, and you can quickly get into very
powerful territory with a custom parser.

Here are some bullet points to keep in mind when implementing your own parser:
  - The key interface here is CLIParser; your parser will crash and burn if it's
    not implemented. See lib.inc for the interface definition, and parsers.inc
    for some examples.
  - The command you'll want is SvnRead::setParser() (note that SvnWrite & family
    do not have any output parsing, because they don't generally have output!).
    You can then pass in the name of the class (as a string), or an already
    instantiated object of your class. Either will work, but doing the latter
    opens up possibilities like chaining multiple outputs through a single
    parser - which is the closest thing svnlib has to piping support right now.

= SVNCOMMAND::CLEAR() =

Set up most of the parameters on a command already and don't want (or aren't
able) to rebuild it after calling SvnCommand::execute()? No prob - just call
SvnCommand::clear(), and it'll selectively flush the contents of the command and
ready it for another execution. Just what gets flushed depends on the flags you
pass in - you can retain command opts and switches, internal switches, and/or
your parser. Returning to the original example, it means after you've called
$info->execute(), you could do the following:

  $info->clear(SvnRead::PRESERVE_CMD_OPTS | SvnRead::PRESERVE_CMD_SWITCHES |
    SvnRead::PRESERVE_INT_SWITCHES | SvnRead::PRESERVE_PARSER)
    ->nonRecursive()->target('trunk/foo')->execute();

After firing the system call specified in the original example, the command
flushes itself out then preps to issue the command with the additions you made:

  svn info --xml --incremental --revision 423 --depth immediate trunk/index.php trunk/example.php@424 trunk/foo

As discussed above, the old --depth opt gets overwritten with your new value
from nonRecursive, and the new target gets added to the end.

See the docblock for SvnCommand::clear() for more details.

= INSTANCE-LEVEL DEFAULTS =

Some options can be set at the level of the SvnInstance, and will automatically
be attached to any commands spawned from that instance.


== CAVEATS ==

- Serious effort has been invested in balancing speed with flexibility for this
  library. All-userspace php can only be so fast, but the real speed bottleneck
  is system calls. If you are concerned about speed, _the most important_ place
  to optimize code that utilizes this library is in minimizing the number of
  times you call SvnCommand::execute(). Cache, queue commands, do whatever it
  takes to minimize those calls.
