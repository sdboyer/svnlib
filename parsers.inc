<?php

/**
 * Pure stream-based parsers.
 */



/**
 * XML-based parsers.
 */

/**
 * A class specifically tailored to parse the incremental xml output of an
 * invocation of `svn info`.
 *
 * @author sdboyer
 *
 */
class SvnInfoXMLParser extends SimpleXMLIterator {

  public function current() {
    $entry = parent::current();
    $item = array();
    $item['url'] = (string) $entry->url;
    $item['repository_root'] = (string) $entry->repository->root;
    $item['repository_uuid'] = (string) $entry->repository->uuid;

    if ($item['url'] == $item['repository_root']) {
      $item['path'] = '/';
    }
    else {
      $item['path'] = substr($item['url'], strlen($item['repository_root']));
    }
    $item['type'] = (string) $entry['kind'];
    $relative_path = (string) $entry['path'];
    $item['rev'] = intval((string) $entry['revision']); // current state of the item
    $item['created_rev'] = intval((string) $entry->commit['revision']); // last edit
    $item['last_author'] = (string) $entry->commit->author;
    $item['time_t'] = strtotime((string) $entry->commit->date);
    return $item;
  }

  /**
   * Override the parent implementation and always return FALSE on hasChildren,
   * because we know we'll never need to recurse.
   */
  public function hasChildren() {
    return FALSE;
  }
}


class SvnLogParser extends IteratorIterator {
  protected $rev;
  public function __construct($xml) {
    parent::__construct(new SimpleXMLIterator($xml));
  }

  public function current() {
    $this->rev = $this->getInnerIterator()->current();
    $revision = array();
    $revision['rev'] = intval((string) $this->rev['revision']);
    $revision['author'] = (string) $this->rev->author;
    $revision['msg'] = rtrim((string) $this->rev->msg); // no trailing linebreaks
    $revision['time_t'] = strtotime((string) $this->rev->date);
    $paths = array();

    foreach ($this->rev->paths->path as $logpath) {
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
    $revision['paths'] = $paths;
    return $revision;
  }

  public function key() {
    $item = $this->getInnerIterator()->current();
    return $item['revision'];
  }
}

class SvnListParser {

}
