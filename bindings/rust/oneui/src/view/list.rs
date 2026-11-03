use super::{PropertyId, ViewError, VmResult};
use crate::sys;
use std::{cell::RefCell, collections::HashSet, rc::Rc};

#[derive(Clone, Debug)]
pub struct ListChange {
    pub(crate) kind: u32,
    pub(crate) before: u64,
    pub(crate) after: u64,
    pub(crate) index: u64,
    pub(crate) to: u64,
    pub(crate) keys: Vec<String>,
    pub(crate) properties: Vec<u32>,
}
impl ListChange {
    pub(crate) fn with_raw<R>(&self, action: impl FnOnce(&sys::OneUiViewListChange) -> R) -> R {
        let keys: Vec<_> = self
            .keys
            .iter()
            .map(|s| sys::OneUiUtf8String::from_str(s))
            .collect();
        action(&sys::OneUiViewListChange {
            struct_size: std::mem::size_of::<sys::OneUiViewListChange>() as u32,
            kind: self.kind,
            before_revision: self.before,
            after_revision: self.after,
            index: self.index,
            count: keys.len() as u64,
            to_index: self.to,
            keys: keys.as_ptr(),
            key_count: keys.len(),
            properties: self.properties.as_ptr(),
            property_count: self.properties.len(),
        })
    }
}

/// Keyed UI-thread collection. UPDATE preserves Rc identity; RESET may replace it.
pub struct StableList<T> {
    entries: Vec<(String, Rc<RefCell<T>>)>,
    revision: u64,
}
impl<T> Default for StableList<T> {
    fn default() -> Self {
        Self {
            entries: Vec::new(),
            revision: 0,
        }
    }
}
impl<T> StableList<T> {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn len(&self) -> usize {
        self.entries.len()
    }
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }
    pub fn revision(&self) -> u64 {
        self.revision
    }
    pub fn get(&self, index: usize) -> Option<&Rc<RefCell<T>>> {
        self.entries.get(index).map(|e| &e.1)
    }
    pub fn key(&self, index: usize) -> Option<&str> {
        self.entries.get(index).map(|e| e.0.as_str())
    }
    pub fn iter(&self) -> impl Iterator<Item = (&str, &Rc<RefCell<T>>)> {
        self.entries
            .iter()
            .map(|(key, value)| (key.as_str(), value))
    }
    pub fn info(&self) -> sys::OneUiViewListInfo {
        sys::OneUiViewListInfo {
            revision: self.revision,
            length: self.len() as u64,
        }
    }
    fn change(
        &mut self,
        kind: u32,
        index: usize,
        to: usize,
        keys: Vec<String>,
    ) -> VmResult<ListChange> {
        let after = self.revision.checked_add(1).ok_or(ViewError::LIMIT)?;
        let change = ListChange {
            kind,
            before: self.revision,
            after,
            index: index as u64,
            to: to as u64,
            keys,
            properties: Vec::new(),
        };
        self.revision = after;
        Ok(change)
    }
    pub fn insert(
        &mut self,
        index: usize,
        key: impl Into<String>,
        item: Rc<RefCell<T>>,
    ) -> VmResult<ListChange> {
        let key = key.into();
        if index > self.len()
            || key.is_empty()
            || key.contains('\0')
            || self.entries.iter().any(|e| e.0 == key)
        {
            return Err(ViewError::BAD_KEY);
        }
        let change = self.change(1, index, 0, vec![key.clone()])?;
        self.entries.insert(index, (key, item));
        Ok(change)
    }
    pub fn remove(&mut self, index: usize) -> VmResult<ListChange> {
        let key = self.key(index).ok_or(ViewError::BAD_KEY)?.to_owned();
        let change = self.change(2, index, 0, vec![key])?;
        self.entries.remove(index);
        Ok(change)
    }
    /// Destination index after removing the source row.
    pub fn move_item(&mut self, index: usize, to: usize) -> VmResult<ListChange> {
        if to >= self.len() {
            return Err(ViewError::BAD_KEY);
        }
        let key = self.key(index).ok_or(ViewError::BAD_KEY)?.to_owned();
        let change = self.change(3, index, to, vec![key])?;
        let value = self.entries.remove(index);
        self.entries.insert(to, value);
        Ok(change)
    }
    /// Call after mutating the existing item's state. Never replaces its Rc.
    pub fn update(&mut self, index: usize, properties: &[PropertyId]) -> VmResult<ListChange> {
        self.update_range(index, 1, properties)
    }
    /// Updates a contiguous group with one notification, retaining every Rc.
    pub fn update_range(
        &mut self,
        index: usize,
        count: usize,
        properties: &[PropertyId],
    ) -> VmResult<ListChange> {
        let end = index.checked_add(count).ok_or(ViewError::BAD_KEY)?;
        if count == 0 || end > self.len() {
            return Err(ViewError::BAD_KEY);
        }
        let keys = self.entries[index..end]
            .iter()
            .map(|(key, _)| key.clone())
            .collect();
        let mut change = self.change(4, index, 0, keys)?;
        change.properties = properties.iter().map(|p| p.get()).collect();
        Ok(change)
    }
    pub fn reset(&mut self, entries: Vec<(String, Rc<RefCell<T>>)>) -> VmResult<ListChange> {
        let mut keys = HashSet::new();
        if entries
            .iter()
            .any(|(key, _)| key.is_empty() || key.contains('\0') || !keys.insert(key))
        {
            return Err(ViewError::BAD_KEY);
        }
        let change = self.change(0, 0, 0, Vec::new())?;
        self.entries = entries;
        Ok(change)
    }

    /// Publish a complete permutation without replacing leases or invalidating fields.
    /// Mutated item fields must send their own notifications, as for MOVE.
    pub fn reorder(&mut self, keys: Vec<String>) -> VmResult<ListChange> {
        let entries: std::collections::HashMap<_, _> = self
            .entries
            .iter()
            .map(|(key, row)| (key.as_str(), row))
            .collect();
        let mut unique = HashSet::new();
        if keys.is_empty()
            || keys.len() != self.len()
            || keys
                .iter()
                .any(|key| !entries.contains_key(key.as_str()) || !unique.insert(key.as_str()))
        {
            return Err(ViewError::BAD_KEY);
        }
        let reordered = keys
            .iter()
            .map(|key| (key.clone(), entries[key.as_str()].clone()))
            .collect();
        let change = self.change(sys::ONEUI_LIST_REORDER, 0, 0, keys)?;
        self.entries = reordered;
        Ok(change)
    }
}

#[cfg(test)]
mod range_tests {
    use super::*;
    #[test]
    fn range_updates_retain_records_and_reject_invalid_bounds_without_a_revision() {
        let mut list = StableList::new();
        let first = Rc::new(RefCell::new(1));
        list.insert(0, "a", first.clone()).unwrap();
        list.insert(1, "b", Rc::new(RefCell::new(2))).unwrap();
        let before = list.revision();
        for (index, count) in [(0, 0), (1, 2), (usize::MAX, 2)] {
            assert!(list.update_range(index, count, &[]).is_err());
            assert_eq!(list.revision(), before);
        }
        let change = list.update_range(0, 2, &[PropertyId::new(1)]).unwrap();
        assert_eq!(change.keys, ["a", "b"]);
        assert_eq!((change.before, change.after), (before, before + 1));
        assert!(Rc::ptr_eq(list.get(0).unwrap(), &first));
    }
}
