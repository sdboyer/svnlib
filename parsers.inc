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
    $item['url'] = (string) $this->entry->url;
    $item['repository_root'] = (string) $this->entry->repository->root;
    $item['repository_uuid'] = (string) $this->entry->repository->uuid;

    if ($item['url'] == $item['repository_root']) {
      $item['path'] = '/';
    }
    else {
      $item['path'] = substr($item['url'], strlen($item['repository_root']));
    }
    $item['type'] = (string) $this->entry['kind'];
    $relative_path = (string) $this->entry['path'];
    $item['rev'] = intval((string) $this->entry['revision']); // current state of the item
    $item['created_rev'] = intval((string) $this->entry->commit['revision']); // last edit
    $item['last_author'] = (string) $this->entry->commit->author;
    $item['time_t'] = strtotime((string) $this->entry->commit->date);
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

class SvnLogXMLParser extends SimpleXMLIterator {

  public function current() {
    $entry = parent::current();
    $revision = array();
    $revision['rev'] = intval((string) $entry['revision']);
    $revision['author'] = (string) $entry->author;
    $revision['msg'] = rtrim((string) $entry->msg); // no trailing linebreaks
    $revision['time_t'] = strtotime((string) $entry->date);
    $paths = array();

    foreach ($entry->paths->path as $logpath) {
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

  public function hasChildren() {
    return FALSE;
  }
}

class SvnListParser {

}
