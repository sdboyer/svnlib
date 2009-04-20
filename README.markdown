# svnlib #

The PHP Subversion library (svnlib) is an all-userspace PHP library that
facilitates easy interaction with the svn binary directly from php code. The
philosophy behind the library is to replicate command line-style behavior as
much as possible; in other words, everything you know about using svn from a
shell/command line will apply to svnlib.

To this end, there are also things that the svnlib does, but intentionally does
not explain. The Subversion documentation is quite good - see the online manual
at http://svnbook.red-bean.com/, or simply the information included in --help.
As such, our documentation assumes you already know (or can find out) about what
the commands actually _do_, and instead focuses on explaining svnlib as an
interface to that functionality.



## Installation ##

Simply unzip/untar/git clone the svnlib somewhere onto your system, and include
in the svn.php file in the root of the directory. Once you include that main
file, svnlib takes care of the rest. 

It may be useful to put the library somewhere on your PHP include path, but YMMV
depending on your use case.



## Basics: How to use it ##

Begin by creating a new svn 'instance' - providing the path to either an svn
working copy, or an svn repository:

    $wc = new SvnWorkingCopy('/path/to/local/working/copy');

Working copies are simpler, so we'll stick with them for this example, but
repository objects are also created by passing in a path, including the
protocol:

    $repo = new SvnRepository('file:///path/to/local/repository');

Note that these two classes do share an abstract parent class, SvnInstance:

             SvnInstance (Abstract)
                        |
                   (Children)
                        |
           |-------------------------|
           |                         |
      SvnWorkingCopy            SvnRepository

Establishing the instance is rather like giving metadata for a database
connection: once the 'database connection'-like data is in your SvnInstance
object, you can run as many svn commands - like database queries - against that
instance as you'd like. The programmatic flow for running commands occurs in
three stages:

 1. Command spawning
 2. Command preparation
 3. Command execution

Commands are spawned by calling the SvnInstance::svn() method and passing in the
name of the subcommand to be invoked:

    $info = $wc->svn('info');

$info now contains an SvnCommand object (a child of the SvnCommand abstract
class, the class diagram is below); in the example, an SvnInfo object. This
command object is then prepared by queueing up parameters using the provided
methods. The method names are all the same as the parameters for svn subcommands
you already know and love:

    // turns on the '--xml' switch
    $info->xml();
    // turns on the '--incremental' switch
    $info->incremental();
    // will pass in the opt '--revision 42'
    $info->revision(42);
    // will pass in the opt '--depth infinity'
    $info->depth('infinity');
    // will pass in 'trunk/index.php', will be interpreted relative to wc base path
    $info->target('trunk/index.php');
    // will pass in 'trunk/example.inc@424', interpreted relative to wc base path
    $info->target('trunk/example.inc', 424);

(NOTE - the readme refers back to this example repeatedly!)

Once you're done passing in parameters, running the command is simple:

    $output = $info->execute();

The svnlib will generate a properly escaped system call in accordance with what
you prepared. In this example, that would be:

    svn info --xml --incremental --revision 42 --depth infinity trunk/index.php trunk/example.php@424

proc_open is then called to fire up the command, and the results are then fed
back into $output.


## Terminology ##

The svnlib does have a bit of jargon. Probably most important is the distinction
between "switch," "opt," "argument," and "parameter." Working from the generated
system call in our example:

 1. Switches: these are command line options that are only a dash and a
    character (or a GNU standard long form); simply passing the letter is
    sufficient to enable the functionality. Switches in the example are
    '--xml' and '--incremental'.
 2. Opts: these are like switches, except the binary (perhaps optionally)
    expects some additional information after the switch. Opts in the example:
    '--revision 42', '--depth infinity'.
 3. Arguments: the input that the binary is expecting to receive; no need to
    prime it with a switch. The distinction between opts and arguments can
    sometimes be a bit academic; certainly, svnlib's underlying handling for
    them is the same.
 4. Parameters: generic term encompassing all three of the above groups. In
    other words, parameters are "all the crap you pass to the command."


Note that I'd be happy to be corrected on these if there's a standard out there
that I missed.


## Internal Architecture Overview ##

There are four essential families of classes that the svnlib employs:

 1. The SvnInstance family; these extend SplFileInfo.
 2. The SvnCommand family; these implement the CLICommand interface.
 3. The SvnOpt family; these implement the CLICommandOpt interface.
 4. The SvnOutputHandler family; these implement the CLIParser interface.

SvnInstance was already dealt with above, so we'll skip over it here.

SvnCommands are svnlib's real workhorses. These are those objects that are
created during "Command Spawning," and they are responsible for assembling the
invocation, firing it, and passing along the output. The class structure looks
generally like this:

                           SvnCommand (Abstract)
                                      |
                                 (Children)
                                      |
                     |--------------------------------|
                     |                                |
             SvnRead (Abstract)               SvnWrite (Abstract)
                     |                                |
                     |                                |
       |------|------|------|------|    |------|------|------|------|
       |      |      |      |      |    |      |      |      |      |
     SvnInfo  |   SvnList   |   (etc.)  |   SvnCopy   |  SvnSwitch  |
            SvnLog       SvnDiff     SvnCommit    SvnExport       (etc.)

So, when we called $wc->svn('info') earlier, it created and returned an SvnInfo
object. The concrete classes for each command tend do very, very little; almost
all of the work is abstacted into the higher-level abstract classes. From client
code's perspective, the most important distinction between the SvnRead and
SvnWrite branches are that output handling and parser objects are only available
with SvnRead.

All of these commands are in commands/svn.commands.inc; check out the source for
further insight.

The SvnOpt family are opts and arguments that are spawned when certain methods
are called from an SvnCommand object. These objects store relevant information
about the opt until execution time, at which point they generate a shell string
using that saved configuration information. The class structure:

                               SvnOpt (Abstract)
                                      |
                                 (Children)
                                      |
        |---------|---------|---------|---------|--------|---------|---------|
        |         |         |         |         |        |         |         |
    SvnOptAccept  | SvnOptChangelist  |    SvnOptDepth   |    SvnOptMessage  |
            SvnOptRevision      SvnOptConfigDir     SvnOptEncoding        (etc.)


Switches are simple enough that they don't need their own objects for handling;
consequently, they are contained entirely within a single bitmask that is stored
in SvnCommand::$cmdSwitches. Again, see commands/svn.commands.inc for a full
list of the switch constants and their associated shell strings.

Note that the various "CLI..." interfaces are all defined in lib.inc, and are a
halfway attempt at abstracting as much of svnlib's approach as possible. The
interfaces will likely mature considerably with time.


## More Complex Behavior (Goodies!) ##

The svnlib has a number of ways that its behavior can be streamlined and
optimized.

### Command Chaining ###

Publicly visible code in svnlib is almost entirely a fluent API; exceptions are
few and far between, mostly documented, and usually intentional (i.e., there
because fluency doesn't make sense in the situation). This means you can chain
commands together. The above $info example could be rewritten in a chain as
follows:

    $output = $info->xml()->incremental()->revision(42)->depth('infinity')
    ->target('trunk/index.php')->target('trunk/example.inc', 424)->execute();

This command sequence is functionally identical to the original example.

All of the command queuing methods are fluent, so they can be chained. Execute,
by default, is not fluent, but instead returns the generated output (at least
for SvnRead-descended subcommands).

### Smart Parameter Queueing/Storage ###

Some parameters can be passed multiple times to some subcommands; others can
only be passed once. The svnlib is aware of this, and handles it mostly
transparently. The primary parameter capable of doing this is 'target,' which
are those paths/to/files/or/directories that you pass in for svn to act on. As
the original example shows, every SvnCommand::target(...) call results in an
additional target argument being generated for the final system call.

In keeping with svnlib's philosophy, of mirroring the command line behavior of
the binary, most parameters can only be passed once. Consequently, svnlib
doesn't document whether or not multiple instances of a parameter are allowed.
For that, consult the svn help documentation.

If an only-allowed-once opt is passed in a second time, it will overwrite the
previous instance of the opt and obliterate any of the config stored therein.
From a fresh $info = $wc->svn('info); object, then:

    $info->revision(21)->target('index.php')->target('.htaccess', 822)->execute();

will generate the system call:

    $ svn --revision 21 index.php .htaccess@822

whereas the commands:

    $info->revision(21)->target('index.php')->target('.htaccess', 822)->revision(111)->execute();

will generate the system call:

    $ svn --revision 111 index.php .htaccess@822

The first revision opt will be overwritten by the later revision opt and never
make it into the call.

### Mass Targeting ###

The --targets command line option allows you to specify a file from which svn
should read the list of targets for command execution. Svnlib leverages this
option by providing an 'aggregation' feature to the standard approach for
queuing a target (SvnCommand::target()), wherein a file for use by --targets
will be dynamically generated using the targets you pass in one at a time to
SvnCommand::target(). Aggregation is enabled by passing TRUE as the third
parameter. Our example again:

    $output = $info->xml()->incremental()->revision(42)->depth('infinity')
      ->target('trunk/index.php', NULL, TRUE)
      ->target('trunk/example.inc', 424, TRUE)->execute();

While the output will be identical to the original example, the system call will
be slightly different:

    $ svn info --xml --incremental --revision 42 --depth infinity --targets /tmp/<random filename>

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

### Fun with Command Switches ###

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

### Custom Output Parsers ###

Svnlib's built in output parsers are, at best, weak. While they do the handy
job of implementing some SPL interfaces that cut down on memory use, they also
present that data in a very contrived fashion that may not suit your needs.
Fortunately, they're easily overridden, and you can quickly get into very
powerful territory with a custom parser.

Here are some bullet points to keep in mind when implementing your own parser:

 *  The key interface here is CLIParser; your parser will crash and burn if it's
    not implemented. See lib.inc for the interface definition, and parsers.inc
    for some examples.
 *  The command you'll want is SvnRead::setParser() (note that SvnWrite & family
    do not have any output parsing, because they don't generally have output!).
    You can then pass in the name of the class (as a string), or an already
    instantiated object of your class. Either will work, but doing the latter
    opens up possibilities like chaining multiple outputs through a single
    parser - which is the closest thing svnlib has to piping support right now.

### SvnCommand::clear() ###

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

    svn info --xml --incremental --revision 42 --depth immediate trunk/index.php trunk/example.php@424 trunk/foo

As discussed above, the old --depth opt gets overwritten with your new value
from nonRecursive, and the new target gets added to the end.

See the docblock for SvnCommand::clear() for more details.

### Path Prefixing ###

### Instance-level Defaults ###

Some options can be set at the level of the SvnInstance, and will automatically
be attached to any commands spawned from that instance.


## Caveats ##

 *  Serious effort has been invested in balancing speed with flexibility for
    this library. All-userspace php can only be so fast, but the real speed
    bottleneck is the system calls. If you are concerned about speed, _the most
    important_ place to optimize code that utilizes this library is in
    minimizing the number of times you call SvnCommand::execute(). Cache, queue
    commands, do whatever it takes to minimize those calls.

## License ##

Copyleft 2009, Sam Boyer (http://samboyer.org)

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2 or later, as published
by the Free Software Foundation.