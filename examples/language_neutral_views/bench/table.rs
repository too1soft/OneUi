use super::{
    paint_probe,
    table_vm::{generated, Model},
};
use oneui::view::{View, ViewMount, ViewNotifier};
use oneui::{Table, TableColumn, Window, WindowOptions};
use std::{
    cell::{Cell, RefCell},
    fs,
    io::Write,
    path::Path,
    rc::Rc,
    time::Instant,
};
enum Page {
    Ir(View, ViewNotifier),
    Manual(Table),
}
pub fn run(
    directory: &Path,
    count: usize,
    rounds: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut raw = std::io::BufWriter::new(fs::File::create(directory.join("table-samples.csv"))?);
    writeln!(raw, "round,path,case,sample,elapsed_ns")?;
    let mut meta = std::io::BufWriter::new(fs::File::create(directory.join("table-runs.csv"))?);
    writeln!(
        meta,
        "round,path,case,construction_ns,cold_paint_ns,paints,vm_strong_after_close"
    )?;
    for round in 0..rounds {
        for group in ["update1000", "insert_remove10", "move10"] {
            for slot in 0..2 {
                let ir = (round + slot) % 2 == 0;
                let name = if ir { "ir" } else { "manual" };
                let start = Instant::now();
                super::diagnostics::reset();
                let window = Window::new(&WindowOptions {
                    title: format!("OneUI paired {name} {group}"),
                    width: 640,
                    height: 600,
                    ..WindowOptions::default()
                })
                .map_err(|e| format!("{e:?}"))?;
                let vm = Rc::new(RefCell::new(Model::default()));
                let mount = ViewMount::new(&window, None)?;
                mount.set_styles("", false)?;
                let page = Rc::new(if ir {
                    let view = View::instantiate(&mount, generated::spec(), vm.clone())?;
                    let notifier = view.notifier()?;
                    Page::Ir(view, notifier)
                } else {
                    let table = Table::new().unwrap();
                    table.set_columns(&[TableColumn {
                        header: "Value".into(),
                        width: 600.0,
                    }]);
                    table.set_row_height(44.0);
                    table.set_header_height(40.0);
                    table.set_column_dividers_visible(false);
                    table.set_rows(&vm.borrow().native());
                    table.set_selected_index(-1);
                    window.set_content(table.as_widget());
                    Page::Manual(table)
                });
                let construction = start.elapsed().as_nanos();
                let dispatcher = window.dispatcher();
                let ready = Rc::new(Cell::new(false));
                let marker = ready.clone();
                let state = vm.clone();
                let engine = page.clone();
                let snapshot = Rc::new(RefCell::new(None));
                let save = snapshot.clone();
                let warmed = directory.join(format!("table-{name}-{group}-warmup.json"));
                let samples = paint_probe::run(
                    &window,
                    if group == "insert_remove10" {
                        (count + 50) * 2
                    } else {
                        count + 50
                    },
                    ready,
                    move |index| {
                        if index == 49 && round == 0 {
                            fs::write(&warmed, dispatcher.layout_snapshot_json().unwrap()).unwrap();
                        }
                        if index == 0 {
                            *save.borrow_mut() = Some(dispatcher.layout_snapshot_json().unwrap());
                        }
                        let case = if group == "insert_remove10" {
                            if index % 2 == 0 {
                                "insert10"
                            } else {
                                "remove10"
                            }
                        } else {
                            group
                        };
                        let changes = state.borrow_mut().mutate(case, index);
                        match &*engine {
                            Page::Ir(_, n) => {
                                for change in &changes {
                                    n.notify_list(generated::ROWS, change).unwrap();
                                }
                            }
                            Page::Manual(t) => {
                                if case == "update1000" {
                                    assert!(t.update_row(
                                        0,
                                        &state.borrow().rows.get(0).unwrap().borrow().native()
                                    ));
                                } else {
                                    t.set_rows(&state.borrow().native());
                                    t.set_selected_index(-1);
                                }
                            }
                        }
                        marker.set(true);
                    },
                );
                let warmup = if group == "insert_remove10" { 100 } else { 50 };
                for (i, value) in samples.ns.iter().skip(warmup).enumerate() {
                    let case = if group == "insert_remove10" {
                        if i % 2 == 0 {
                            "insert10"
                        } else {
                            "remove10"
                        }
                    } else {
                        group
                    };
                    let index = if group == "insert_remove10" { i / 2 } else { i };
                    writeln!(raw, "{round},{name},{case},{index},{value}")?;
                }
                if round == 0 {
                    fs::write(
                        directory.join(format!("table-{name}-{group}.json")),
                        snapshot.borrow().as_ref().unwrap(),
                    )?;
                }
                drop(page);
                drop(mount);
                super::diagnostics::save(directory, name, group);
                writeln!(
                    meta,
                    "{round},{name},{group},{construction},{},{},{}",
                    samples.cold_paint_ns,
                    samples.paints,
                    Rc::strong_count(&vm)
                )?;
                raw.flush()?;
                meta.flush()?;
                println!(
                    "benchmark round {round} {name} {group}: {} samples",
                    samples.ns.len() - warmup
                );
            }
        }
    }
    Ok(())
}
