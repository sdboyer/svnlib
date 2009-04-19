<?php

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

abstract class SvnOutputHandler extends IteratorIterator implements CLIParser {
  protected $output;
  

  public function __construct() {

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
}

/**
 * A class specifically tailored to parse the incremental xml output of an
 * invocation of `svn info`.
 *
 * @author sdboyer
 *
 */
class SvnInfoXMLParser extends SvnOutputHandler implements SeekableIterator {
  protected $raw;

  public function parseOutput() {
    rewind($this->output);
    $this->raw = stream_get_contents($this->output);
    parent::__construct(new SimpleXMLIterator($this->raw));
  }

  protected function parse($entry) {
    $item = array(
      'url'             => (string) $entry->url,
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
    // $relative_path = (string) $entry['path'];
    return $item;
  }

  public function seekDouble($rev = NULL, $path = NULL) {
    $items = $query = array();
    if (!is_null($rev)) {
      $query[] = "@revision = '$rev'";
    }
    if (!is_null($path)) {
      $query[] = "@path = '$rev'";
    }
    elseif (empty($query)) {
      throw new Exception('No arguments provided for xpath query.', E_RECOVERABLE_ERROR);
    }
    foreach ($this->getInnerIterator()->xpath('/info/entry[' . implode(' and ', $query) . ']') as $entry) {
      $item = $this->parse($entry);
      $items[$item['path']] = $item;
    }
    return $items;
  }

  public function seek($rev) {
    $items = array();
    foreach ($this->parse($this->getInnerIterator()->xpath("/info/entry[@revision='$rev']")) as $item) {
      $items[$item['path']] = $item;
    }
    return $items;
  }

  public function clear() {
    unset($this->raw);
  }
}


class SvnLogParser extends SvnOutputHandler implements SeekableIterator {
  protected $raw;
  protected $rev;
//  public function __construct($xml) {
//    parent::__construct(new SimpleXMLIterator($xml));
//  }
//  public function current() {
//    return $this->parse($this->getInnerIterator()->current());
//  }

  public function parseOutput() {
    rewind($this->output);
    $this->raw = stream_get_contents($this->output);
    parent::__construct(new SimpleXMLIterator($this->raw));
  }

  protected function parse($rev) {
    $paths = array();
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
      $paths[$path['path']] = $path;
    }

    $revision = array(
      'rev'     => intval((string) $rev['revision']),
      'author'  => (string) $rev->author,
      'msg'     => rtrim($rev->msg), // no trailing linebreaks
      'time_t'  => strtotime($rev->date),
      'paths'   => $paths,
    );
    return $revision;
  }

  public function seek($position) {
    return $this->parse($this->getInnerIterator()->xpath("/log/logentry[@revision='$position']"));
  }

  public function clear() {
    unset($this->raw);
  }
}

class SvnListParser {

}
