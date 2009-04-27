<?php

/*
 * The LIBXML_COMPACT flag can supposedly improve php's libxml performance, but
 * only if system's version of LIBXML is 2.6.21 or newer.
 */

define('SVNLIB_LIBXML_FLAGS', LIBXML_VERSION >= 20621 ? LIBXML_COMPACT | LIBXML_NONET : LIBXML_NONET);

/**
 * Dummy parser class that transparently passes output straight back without
 * any modification, but still implements the CLIParser interface so as to
 * reduce complexity in CLICommand implementations.
 */
class DummyParser implements CLIParser {
  protected $output;

  public function __construct() {}
  public function openOutputHandle() {
    $this->output = fopen('php://temp', 'rw');
    return $this->output;
  }

  public function parseOutput() {
    rewind($this->output);
    return stream_get_contents($this->output);
  }

  public function clear() {}

  public function procClose($destruct = FALSE) {
    if (is_resource($this->output)) {
      fclose($this->output);
    }
  }
}

/**
 * XML-based parsers.
 */

abstract class SvnXMLOutputHandler extends IteratorIterator implements CLIParser {
  protected $output;
  
  /**
   * Override the IteratorIterator constructor so that it's possible to create
   * the object before we have output for it to parse and make iterable.
   */
  public function __construct() {

  }

  /**
   * Delayed backdoor into the IteratorIterator constructor.
   * @param Iterator $it
   */
  final public function build(Iterator $it) {
    parent::__construct($it);
  }

  public function parseOutput() {
    rewind($this->output); // superfluous?
    $this->build(new SimpleXMLIterator(stream_get_contents($this->output)), SVNLIB_LIBXML_FLAGS);
    return $this;
  }

  public function current() {
    return $this->parse($this->getInnerIterator()->current());
  }

  public function openOutputHandle() {
    $this->output = fopen('php://temp', 'rw');
    return $this->output;
  }

  // abstract public function parseOutput();
  abstract protected function parse($item);

  public function procClose($destruct = FALSE) {
    // FIXME: this check should probably be made superfluous, but it's throwing an error atm.
    if (is_resource($this->output)) {
      fclose($this->output);
    }
  }

  public function clear() {}
}

/**
 * A class specifically tailored to parse the incremental xml output of an
 * invocation of `svn info`.
 *
 * @author sdboyer
 *
 */
class SvnInfoXMLParser extends SvnXMLOutputHandler implements SeekableIterator {

  protected function parse($entry, $decode = FALSE) {
    $item = array(
      'url'             => $decode ? rawurldecode($entry->url) : (string) $entry->url,
      'repository_root' => (string) $entry->repository->root,
      'repository_uuid' => (string) $entry->repository->uuid,
      'type'            => (string) $entry['kind'],
      'rev'             => intval((string) $entry['revision']), // current state of the item
      'created_rev'     => intval((string) $entry->commit['revision']), // last edit
      'last_author'     => (string) $entry->commit->author,
      'time_t'          => strtotime((string) $entry->commit->date),
    );
    if ($item['url'] == $item['repository_root']) {
      $item['path'] = '/';
    }
    else {
      $item['path'] = substr($item['url'], strlen($item['repository_root']));
    }
    return $item;
  }

  public function seek($args) {
    if (isset($args['rev'], $args['path'])) {
      return $this->seekBoth($args['rev'], $args['path']);
    }
    elseif (isset($args['rev']) xor isset($args['path'])) {
      return isset($args['rev']) ? $this->seekRev($args['rev']) : $this->seekPath($args['path']);
    }
    throw new Exception('No arguments provided for xpath query.', E_RECOVERABLE_ERROR);
  }

  public function seekRev($rev) {
    $items = array();
    // foreach ($this->parse($this->getInnerIterator()->xpath("/info/entry[@revision='$rev']")) as $item) {
    foreach ($this->getInnerIterator()->xpath("/info/entry[@revision='$rev']") as $entry) {
      $item = $this->parse($entry, TRUE);
      $items[$item['path']] = $item;
    }
    return $items;
  }

  public function seekPath($path) {
    $items = array();
    // foreach ($this->parse($this->getInnerIterator()->xpath("/info/entry[@path='$path']")) as $item) {
    foreach ($this->getInnerIterator()->xpath("/info/entry[url = '$path']") as $entry) {
      $item = $this->parse($entry, TRUE);
      $items[$item['rev']] = $item;
    }
    return $items;
  }

  public function seekBoth($rev, $path) {
//    foreach ($this->getInnerIterator()->xpath("/info/entry[@revision='$rev'][url=concat(.//root,'/','$path')]") as $entry) {
//    foreach ($this->getInnerIterator()->xpath("/info/entry[@revision='$rev']/url[contains(.,'$path')]/..") as $entry) {
    foreach ($this->getInnerIterator()->xpath("/info/entry[@revision='$rev' and url='$path']") as $entry) {
      $item = $this->parse($entry, TRUE);
      if (!$item['url'] == $item['repository_root'] . DIRECTORY_SEPARATOR . $path) {
        continue;
      }
      return $item;
    }
  }
}


class SvnLogXMLParser extends SvnXMLOutputHandler implements SeekableIterator {

  protected function parse($rev) {
    $paths = array();
    $revision = array(
      'rev'     => intval((string) $rev['revision']),
      'author'  => (string) $rev->author,
      'msg'     => rtrim($rev->msg), // no trailing linebreaks
      'time_t'  => strtotime($rev->date),
      'paths'   => &$paths,
    );

    if (is_object($rev->paths)) {
      foreach ($rev->paths->path as $logpath) {
        $path = array(
          'path' => (string) $logpath,
          'action' => (string) $logpath['action'],
        );
        if (!empty($logpath['copyfrom-path'])) {
          $path['copyfrom'] = array(
            'path' => (string) $logpath['copyfrom-path'],
            'rev' => (string) $logpath['copyfrom-rev'],
          );
        }
        elseif ($path['action'] == 'D') {
          // First, do the quick-n-easy check to see if this is a move action.
          $move = $rev->xpath("paths/path[@action = 'A' and @copyfrom-path = '$path[path]");
          if (!empty($move)) {
            continue;
          }
          // On delete actions, check if this is one of the annoying kind. Run
          // an xpath query, checking for an init-substring match from all of
          // the paths in this rev that are type 'A' and have copyfrom info.
          // $parents = $rev->xpath("paths/path[@action = 'A' and @copyfrom-rev and @copyfrom-path != '$path[path]' and starts-with('$path[path]',.)]");
          $parents = $rev->xpath("paths/path[@action = 'A' and @copyfrom-rev and starts-with('$path[path]',.)]");
          if (count($parents) > 1) {
            throw new Exception("Multiple possible copy+delete parents found for $path[path]@$revision[rev]; cannot currently handle this.", E_ERROR);
          }
          else {
            $add_path = array_shift($parents);
            if (empty($add_path)) {
              continue;
            }
            elseif (strpos($path['path'], (string) $add_path) !== FALSE) {
              $path['delfrom'] = array(
                'path' => $add_path['copyfrom-path'] . substr($path['path'], strlen($add_path)),
                'rev' => (string) $add_path['copyfrom-rev'],
              );
            }
            else {
              throw new Exception('Blargh', E_ERROR);
            }
          }
        }
        $paths[(string) $logpath] = $path;
      }
    }
    return $revision;
  }

  public function seek($position) {
    return $this->parse(current($this->getInnerIterator()->xpath("/log/logentry[@revision='$position']")));
  }
}

class SvnListParser {

}
