README for svnlib

The PHP svn library (svnlib) is an all-userspace PHP library that facilitates
easy interaction with the svn binary directly from php code. The philosophy
behind the library is to replicate command line-style behavior as much as
possible; in other words, everything you know about using svn from a
shell/command line will apply to svnlib.

BASICS: HOW TO USE IT

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

MORE COMPLEX BEHAVIOR

The svnlib has a number of ways that its behavior can be streamlined and optimized.

CAVEATS

- Serious effort has been invested in balancing speed with flexibility for this
  library. All-userspace php can only be so fast, but the real speed bottleneck
  is the system calls themselves. If you are concerned about speed, _the most
  important_ place to optimize code that utilizes this library is in minimizing
  the number of times you call SvnCommand::execute(). Cache, queue commands, do
  whatever it takes to minimize those calls.
