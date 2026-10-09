use super::*;

impl<K: Clone + Ord> DockNode<K> {
    pub fn split(axis: DockAxis, ratio: f32, first: Self, second: Self) -> Result<Self, DockError> {
        let node = Self::Split {
            axis,
            ratio,
            first: Box::new(first),
            second: Box::new(second),
        };
        native::validate(&native::root_state(node.clone()))?;
        Ok(node)
    }
    // Traversal only; mutations and geometry are delegated to the SDK model.
    pub fn ids(&self) -> Vec<K> {
        match self {
            Self::Leaf(id) => vec![id.clone()],
            Self::Split { first, second, .. } => {
                let mut ids = first.ids();
                ids.extend(second.ids());
                ids
            }
        }
    }
    pub fn contains(&self, id: &K) -> bool {
        match self {
            Self::Leaf(key) => key == id,
            Self::Split { first, second, .. } => first.contains(id) || second.contains(id),
        }
    }
    pub fn without(self, id: &K) -> Option<Self> {
        if !self.contains(id) {
            return Some(self);
        }
        native::edit(
            &native::root_state(self),
            0,
            0,
            Some(id),
            None,
            empty_rect(),
            0.,
            &[],
        )
        .ok()?
        .0
        .root
    }
    pub fn swap(&mut self, first: &K, second: &K) -> Result<(), DockError> {
        let (state, _) = native::edit(
            &native::root_state(self.clone()),
            0,
            12,
            Some(first),
            Some(second),
            empty_rect(),
            0.,
            &[],
        )?;
        *self = state.root.ok_or(DockError::MissingPane)?;
        Ok(())
    }
    pub fn balance(&mut self) {
        if let Ok((state, _)) = native::edit(
            &native::root_state(self.clone()),
            0,
            13,
            None,
            None,
            empty_rect(),
            0.,
            &[],
        ) {
            if let Some(root) = state.root {
                *self = root;
            }
        }
    }
    pub fn set_ratio(&mut self, path: &[bool], ratio: f32) -> Result<(), DockError> {
        let (state, _) = native::edit(
            &native::root_state(self.clone()),
            0,
            14,
            None,
            None,
            empty_rect(),
            ratio,
            path,
        )?;
        *self = state.root.ok_or(DockError::InvalidTarget)?;
        Ok(())
    }
    /// Minimum content extent, using the same nonnegative policy as geometry.
    pub fn minimum(&self, gap: f32, policy: &impl Fn(&K) -> PaneMinimum) -> PaneMinimum {
        native::minimum(self, gap, policy)
    }
    pub fn geometry(
        &self,
        bounds: Rect,
        gap: f32,
        policy: impl Fn(&K) -> PaneMinimum,
    ) -> Result<DockGeometry<K>, DockError> {
        native::geometry(self, bounds, gap, policy)
    }
}
