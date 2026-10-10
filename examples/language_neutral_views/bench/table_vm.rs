//! Identical keyed data and mutation sequence for the two native table paths.
use oneui::view::{ListChange, StableList};
use std::{cell::RefCell, rc::Rc};
pub mod generated {
    include!(concat!(env!("OUT_DIR"), "/table_probe.rs"));
}
use generated::*;
pub struct CellVm {
    value: String,
}
impl oneui_v1_StringItemVm for CellVm {
    fn id(&self) -> &str {
        "value"
    }
    fn value(&self) -> &str {
        &self.value
    }
}
pub struct Row {
    id: String,
    cells: StableList<CellVm>,
}
impl Row {
    fn new(id: String) -> Rc<RefCell<Self>> {
        let mut cells = StableList::new();
        cells
            .insert(
                0,
                "value",
                Rc::new(RefCell::new(CellVm { value: id.clone() })),
            )
            .unwrap();
        Rc::new(RefCell::new(Self { id, cells }))
    }
    pub fn native(&self) -> oneui::TableRow {
        oneui::TableRow {
            cells: vec![self.cells.get(0).unwrap().borrow().value.clone()],
        }
    }
}
impl oneui_v1_RowVm for Row {
    type CellsItem = CellVm;
    fn id(&self) -> &str {
        &self.id
    }
    fn cells(&self) -> &StableList<CellVm> {
        &self.cells
    }
}
pub struct Column;
impl oneui_v1_ColumnVm for Column {
    fn action(&self) -> bool {
        false
    }
    fn alignment(&self) -> i64 {
        0
    }
    fn header(&self) -> &str {
        "Value"
    }
    fn id(&self) -> &str {
        "value"
    }
    fn kind(&self) -> i64 {
        0
    }
    fn width(&self) -> f64 {
        600.0
    }
}
pub struct Model {
    columns: StableList<Column>,
    pub rows: StableList<Row>,
}
impl Default for Model {
    fn default() -> Self {
        let mut columns = StableList::new();
        columns
            .insert(0, "value", Rc::new(RefCell::new(Column)))
            .unwrap();
        let mut rows = StableList::new();
        rows.reset(
            (0..1000)
                .map(|i| {
                    let id = format!("row-{i:04}");
                    (id.clone(), Row::new(id))
                })
                .collect(),
        )
        .unwrap();
        Self { columns, rows }
    }
}
impl TableProbeVm for Model {
    type ColumnsItem = Column;
    type RowsItem = Row;
    fn columns(&self) -> &StableList<Column> {
        &self.columns
    }
    fn rows(&self) -> &StableList<Row> {
        &self.rows
    }
}
impl Model {
    pub fn native(&self) -> Vec<oneui::TableRow> {
        self.rows.iter().map(|(_, r)| r.borrow().native()).collect()
    }
    pub fn mutate(&mut self, case: &str, index: usize) -> Vec<ListChange> {
        match case {
            "update1000" => {
                let row = self.rows.get(0).unwrap();
                row.borrow().cells.get(0).unwrap().borrow_mut().value =
                    format!("value-{}", index % 2);
                // Nested cells retain their VM identities. Mark their value cache dirty
                // by invalidating the row's list-valued property through UPDATE.
                vec![self
                    .rows
                    .update(0, &[oneui_v1_Row_properties::CELLS])
                    .unwrap()]
            }
            "insert10" => (0..10)
                .map(|i| {
                    let id = format!("added-{index}-{i}");
                    self.rows.insert(i, id.clone(), Row::new(id)).unwrap()
                })
                .collect(),
            "remove10" => (0..10).map(|_| self.rows.remove(0).unwrap()).collect(),
            "move10" => (0..10)
                .map(|_| self.rows.move_item(0, 999).unwrap())
                .collect(),
            _ => unreachable!(),
        }
    }
}
