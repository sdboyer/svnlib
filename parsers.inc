<?php


/**
 * A class specifically tailored to parse the incremental xml output of an
 * invocation of `svn info`.
 *
 * @author sdboyer
 *
 */
class SvnInfoParser extends SimpleXMLIterator {

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
    // $i = 'break on me';
    return $item;
  }

  /**
   * Override the parent implementation and always return FALSE on hasChildren,
   * because we know that we never want or need to recurse.
   */
  public function hasChildren() {
    return FALSE;
  }
}