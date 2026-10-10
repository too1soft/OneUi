use std::collections::{BTreeMap, BTreeSet};
#[derive(Default)]
pub struct Broadcast {
    pub source: Option<String>,
    pub receivers: BTreeSet<String>,
    pub paused: bool,
    pub disconnected: BTreeSet<String>,
    pub received: BTreeMap<String, String>,
    pub pending: Option<(String, String)>,
}
impl Broadcast {
    pub fn toggle(&mut self, pane: &str, active: &str, visible: &[String]) {
        if self.source.as_deref() == Some(pane) {
            self.stop();
            return;
        }
        if self.receivers.remove(pane) {
            if self.receivers.is_empty() {
                self.stop();
            }
            return;
        }
        if self.disconnected.contains(pane) {
            return;
        }
        if self.source.is_some() {
            self.receivers.insert(pane.into());
            return;
        }
        if visible.len() < 2 || self.disconnected.contains(active) {
            return;
        }
        self.source = Some(active.into());
        self.paused = false;
        if pane == active {
            self.receivers.extend(
                visible
                    .iter()
                    .filter(|p| p.as_str() != active && !self.disconnected.contains(*p))
                    .cloned(),
            );
        } else {
            self.receivers.insert(pane.into());
        }
        if self.receivers.is_empty() {
            self.stop();
        }
    }
    pub fn stop(&mut self) {
        self.source = None;
        self.receivers.clear();
        self.paused = false;
        self.pending = None;
    }
    pub fn input(&mut self, pane: &str, text: &str, paste: bool) -> bool {
        if self.disconnected.contains(pane) {
            return false;
        }
        if paste && text.contains('\n') && self.source.as_deref() == Some(pane) && !self.paused {
            self.pending = Some((pane.into(), text.into()));
            return false;
        }
        self.received.entry(pane.into()).or_default().push_str(text);
        if self.receivers.contains(pane) {
            self.paused = true;
        }
        if self.source.as_deref() == Some(pane) && !self.paused {
            for receiver in &self.receivers {
                if !self.disconnected.contains(receiver) {
                    self.received
                        .entry(receiver.clone())
                        .or_default()
                        .push_str(text);
                }
            }
        }
        true
    }
    pub fn confirm(&mut self, accept: bool) {
        if let Some((pane, text)) = self.pending.take() {
            if accept {
                self.input(&pane, &text, false);
            }
        }
    }
    pub fn disconnect(&mut self, pane: &str) {
        if !self.disconnected.remove(pane) {
            self.disconnected.insert(pane.into());
            if self.source.as_deref() == Some(pane) || self.receivers.contains(pane) {
                self.paused = true;
            }
        }
    }
    pub fn role(&self, pane: &str) -> &str {
        if self.source.as_deref() == Some(pane) {
            "source"
        } else if self.receivers.contains(pane) {
            "receiver"
        } else {
            "none"
        }
    }
    pub fn hint(&self, pane: &str, active: &str) -> &str {
        match self.role(pane) {
            "source" => "广播源：此窗格的输入会同步到其他成员 · 单击结束广播",
            "receiver" => "正在接收广播 · 单击移出广播",
            _ => {
                if self.source.is_some() {
                    "加入广播，接收广播源的输入"
                } else if pane == active {
                    "从此窗格广播到本标签的所有窗格"
                } else {
                    "从当前窗格广播到此窗格"
                }
            }
        }
    }
}
